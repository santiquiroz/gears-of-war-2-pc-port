"""Draft workaround restoring the War Journal notification glyph; refuses active L bindings.
A proper upstream fix should allocate L a distinct glyph and rebuild the layout instead.
"""
from pathlib import Path
from PIL import Image,ImageChops
import json,re,subprocess,shutil,hashlib
import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game-root',type=Path,required=True)
parser.add_argument('--repo-root',type=Path,default=Path(__file__).resolve().parents[2])
parser.add_argument('--pack',type=Path,required=True,help='Pack title folder containing replace and dump folders')
parser.add_argument('--work-dir',type=Path,required=True,help='Output directory outside the source checkout')
parser.add_argument('--apply',action='store_true',help='Opt in to writing generated files to the local game; game must be closed')
args=parser.parse_args()
root=args.game_root.resolve();lab=args.repo_root.resolve();pack=args.pack.resolve();work=args.work_dir.resolve()
if work==lab or lab in work.parents:raise SystemExit('--work-dir must be outside the source checkout (generated game assets must not be committed)')
work.mkdir(parents=True,exist_ok=True)
backup=work/'backup';backup.mkdir(exist_ok=True)
if args.apply:
    active=subprocess.run(['tasklist','/FI','IMAGENAME eq gow2.exe','/FO','CSV','/NH'],capture_output=True,text=True)
    if '"gow2.exe"' in active.stdout.lower():raise SystemExit('Close GoW2 before --apply')
config=(root/'gow2.toml').read_text(encoding='utf-8-sig')
for line in config.splitlines():
 if re.match(r'^keybind_',line):
  value=line.split('=',1)[1].split('#',1)[0].strip().strip('"')
  assert 'L' not in re.split(r'[,+]',value), 'L binding needs a separate keycap before repairing the collision'
info=json.loads((lab/'tools/keycaps/font_info.json').read_text())
x,y,w,h,page,vo=info['cells'][ord('-')];pagehash=info['page_hashes'][page]
original=Image.open(next((pack/'dump').glob(pagehash+'*.dds'))).convert('RGBA')
scale=original.width//256
badge=original.crop((x*scale,y*scale,(x+w)*scale,(y+h)*scale))
# L is stored in the '-' font slot by this release's generated keycap table.
# Preserve the runtime table/ABI and every active bound prompt; restore that slot's notification art.
target_cell=(44,22,22,22)
records=[]
for variant in ['replace','replace_kbdA','replace_kbdB','replace_ps4','replace_ps5']:
 target=pack/variant/(pagehash+'.dds')
 if not target.exists():continue
 old=Image.open(target).convert('RGBA');s=old.width//256
 tx,ty,tw,th=target_cell;box=(tx*s,ty*s,(tx+tw)*s,(ty+th)*s)
 patched=old.copy();patched.paste((0,0,0,0),box)
 factor=min(tw*s/badge.width,th*s/badge.height)
 icon=badge.resize((round(badge.width*factor),round(badge.height*factor)),Image.Resampling.LANCZOS)
 patched.paste(icon,(tx*s+(tw*s-icon.width)//2,ty*s+(th*s-icon.height)//2))
 difference=ImageChops.difference(old,patched);difference.paste((0,0,0,0),box)
 assert difference.getbbox(alpha_only=False) is None
 output_dir=work/variant;output_dir.mkdir(exist_ok=True)
 png=output_dir/(pagehash+'.png');patched.save(png)
 subprocess.run([str(root/'tools/texconv/texconv.exe'),'-nologo','-dx10','-y','-m','0','-f','R8G8B8A8_UNORM','-o',str(output_dir),str(png)],check=True,capture_output=True)
 output=output_dir/(pagehash+'.dds')
 assert output.read_bytes()[84:88]==b'DX10', 'DX10 DDS header required for variant switching'
 before=backup/variant/target.name
 if args.apply and not before.exists():before.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(target,before)
 if args.apply:shutil.copy2(output,target)
 records.append({'destination':str(target),'prepared':str(output),'backup':str(before),'outside_cell_unchanged':True})
assert len(records)>=3
(work/'prepared.json').write_text(json.dumps(records,indent=2))
preview=badge.resize((192,208),Image.Resampling.NEAREST);preview.save(work/'notification-preview.png')
print(json.dumps({'prepared_variants':len(records),'active_L_binding':False,'only_notification_cell_changed':True}))