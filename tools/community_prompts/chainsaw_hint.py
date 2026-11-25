"""Generate keyboard-only chainsaw tutorial keycaps from the user's own 128x256 dump.
Draft reconstruction of the local fix; regenerate after changing keybind_b.
"""
from pathlib import Path
import re,json,subprocess,shutil
from PIL import Image,ImageChops
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
m=re.search(r'^keybind_b\s*=\s*"([^"]*)"',config,re.M)
key=(m.group(1) if m else 'F,V').split(',')[0].strip()
layout=(lab/'src/gow2_keycap_layout.inc').read_text()
m=re.search(r"\{u'\\x[0-9a-f]+', (\d+), (\d+), (\d+), (\d+), (\d+), \d+\},  // "+re.escape(key)+r'$',layout,re.M)
if not m:raise SystemExit('Unsupported key binding: '+key)
page,x,y,w,h=map(int,m.groups());info=json.loads((lab/'tools/keycaps/font_info.json').read_text())
font=Image.open(pack/'replace'/(info['page_hashes'][page]+'.dds')).convert('RGBA');scale=font.width//256
cap=font.crop((x*scale,y*scale,(x+w)*scale,(y+h)*scale))
files=sorted((pack/'dump').glob('0be50236e9d991cd_*.dds'))
if len(files)!=1:raise SystemExit('Need exactly one original chainsaw texture dump')
original=Image.open(files[0]).convert('RGBA')
if original.size!=(128,256):raise SystemExit('Unexpected art revision/size; skipped')
modified=original.copy();boxes=[(98,0,128,32),(98,32,128,64)]
for left,top,right,bottom in boxes:
 modified.paste((0,0,0,0),(left,top,right,bottom))
 icon=cap.copy();icon.thumbnail((right-left-4,bottom-top-4),Image.Resampling.LANCZOS)
 modified.paste(icon,(left+(right-left-icon.width)//2,top+(bottom-top-icon.height)//2))
diff=ImageChops.difference(original,modified)
for box in boxes:diff.paste((0,0,0,0),box)
assert diff.getbbox(alpha_only=False) is None
png=work/'0be50236e9d991cd.png';modified.save(png)
subprocess.run([str(root/'tools/texconv/texconv.exe'),'-nologo','-dx10','-y','-m','0','-f','R8G8B8A8_UNORM','-o',str(work),str(png)],check=True,capture_output=True)
output=work/'0be50236e9d991cd.dds'
assert output.read_bytes()[84:88]==b'DX10'
for variant in ['replace_kbdA','replace_kbdB']:
 if args.apply:
  target=pack/variant/output.name;target.parent.mkdir(exist_ok=True)
  if target.exists():
   dest=backup/variant/target.name;dest.parent.mkdir(parents=True,exist_ok=True)
   if not dest.exists():shutil.copy2(target,dest)
  shutil.copy2(output,target)
print(json.dumps({'key':key,'generated':True,'applied':args.apply,'outside_boxes_unchanged':True}))
