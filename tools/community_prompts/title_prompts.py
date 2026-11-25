"""Generate local keyboard Start/Back font variants and an English neutral title label.
Draft recipe, not an automatic runtime rebind implementation. Outputs require the user's own assets.
"""
from pathlib import Path
import struct,json,shutil,re,subprocess,hashlib
from PIL import Image
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
info=json.loads((lab/'tools/keycaps/font_info.json').read_text())
layout=(lab/'src/gow2_keycap_layout.inc').read_text()
config=(root/'gow2.toml').read_text(encoding='utf-8-sig')
manifest=[]
def save_before(path):
 relative=str(path.relative_to(root)) if path.is_relative_to(root) else str(path)
 target=backup/'files'/relative
 existed=path.exists()
 if existed and not target.exists():
  target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
 manifest.append({'path':str(path),'existed':existed,'backup':str(target) if existed else None})
def binding(name,fallback):
 m=re.search(r'^'+re.escape(name)+r'\s*=\s*"([^"]*)"',config,re.M)
 return (m.group(1) if m else fallback).split(',')[0].strip()
def cap(key):
 m=re.search(r"\{u'\\x[0-9a-f]+', (\d+), (\d+), (\d+), (\d+), (\d+), \d+\},  // "+re.escape(key)+r'$',layout,re.M)
 if not m: raise ValueError('Unsupported binding: '+key)
 page,x,y,w,h=map(int,m.groups())
 im=Image.open(pack/'replace'/(info['page_hashes'][page]+'.dds')).convert('RGBA')
 scale=im.width//256
 return im.crop((x*scale,y*scale,(x+w)*scale,(y+h)*scale))
pagehash=info['page_hashes'][0]
base=Image.open(pack/'replace'/(pagehash+'.dds')).convert('RGBA');scale=base.width//256
modified=base.copy()
# The shell character is shared by the two-part Start and Back icons. Give both a complete keycap.
for character,key in [('\\',None),('>',binding('keybind_start','Return')),('<',binding('keybind_back','Tab'))]:
 x,y,w,h,page,vo=info['cells'][ord(character)];assert page==0
 box=(x*scale,y*scale,(x+w)*scale,(y+h)*scale)
 modified.paste((0,0,0,0),box)
 if key:
  image=cap(key)
  fit=min(w*scale/image.width,h*scale/image.height)
  image=image.resize((round(image.width*fit),round(image.height*fit)),Image.Resampling.LANCZOS)
  modified.paste(image,(x*scale+(w*scale-image.width)//2,y*scale+(h*scale-image.height)//2))
# Exact pixel isolation: only the three legacy cells may differ from the original.
allowed=Image.new('1',base.size)
for c in ['\\','>','<']:
 x,y,w,h,_,_=info['cells'][ord(c)];allowed.paste(1,(x*scale,y*scale,(x+w)*scale,(y+h)*scale))
from PIL import ImageChops
outside=ImageChops.difference(base,modified);outside.paste((0,0,0,0),mask=allowed)
assert outside.getbbox(alpha_only=False) is None
png=work/(pagehash+'.png');modified.save(png)
subprocess.run([str(root/'tools/texconv/texconv.exe'),'-nologo','-dx10','-y','-m','0','-f','R8G8B8A8_UNORM','-o',str(work),str(png)],check=True,capture_output=True)
output=work/(pagehash+'.dds')
assert output.read_bytes()[84:88]==b'DX10', 'DX10 DDS header required for variant switching'
assert Image.open(output).size==base.size
for variant in ['replace_kbdA','replace_kbdB']:
 target=pack/variant/(pagehash+'.dds')
 if args.apply:save_before(target);shutil.copy2(output,target)
# The label names the action neutrally; the icon now follows the active font texture variant.
# Keep the original font markup, so the controller's own original/PS variants still apply.
source=root/'GearGame/CookedXenon/Coalesced_int.bin'
if args.apply:save_before(source)
data=source.read_bytes();p=0;changes=[]
def integer():
 global p
 value=struct.unpack_from('>i',data,p)[0];p+=4;return value
def string():
 global p
 begin=p;n=integer();size=abs(n)*(2 if n<0 else 1)
 value=data[p:p+size-(2 if n<0 else 1)].decode('utf-16-le' if n<0 else 'latin-1') if n else ''
 p+=size;return value,begin,p,n
for _ in range(integer()):
 filename,*_=string()
 for _ in range(integer()):
  section,*_=string()
  for _ in range(integer()):
   key,*_=string();value,start,end,n=string()
   if section=='GearTitleScreen' and key=='PressStartLabel':
    desired='Press <Fonts:Warfare_HUD.Xbox360_18pt>\\><Fonts:/>'
    assert value.rstrip() in ['Press START (<Fonts:Warfare_HUD.Xbox360_18pt>\\><Fonts:/>)',desired],value
    desired=desired.ljust(len(value))
    raw=(desired+'\0').encode('utf-16-le' if n<0 else 'latin-1')
    size=len(desired)+1
    changes.append((start,end,struct.pack('>i',-size if n<0 else size)+raw,value,desired))
assert len(changes)==1
out=bytearray(data[:p])
for start,end,raw,_,_ in reversed(changes):out[start:end]=raw
assert len(out)<=len(data)
out.extend(bytes(len(data)-len(out)));(work/'Coalesced_int.bin').write_bytes(out)
if args.apply:source.write_bytes(out)
# Invalidate just this generated language cache. The game rebuilds it and registers its correct SHA-1.
stamp=Path.home()/'AppData/Local/GearsOfWar2PC/cache/coalesced/Coalesced_int.bin.stamp'
if args.apply and stamp.exists():stamp.unlink()
result={'keyboard_start':binding('keybind_start','Return'),'keyboard_back':binding('keybind_back','Tab'),'source_size_preserved':len(out)==len(data),'font_changes_isolated':True,'files':manifest}
(work/'result.json').write_text(json.dumps(result,indent=2))
(backup/'manifest.json').write_text(json.dumps(result,indent=2))
# Human review strip, generated from the exact installed glyphs.
preview=Image.new('RGBA',(400,120),(25,25,25,255))
preview.alpha_composite(cap(binding('keybind_start','Return')),(10,12))
preview.alpha_composite(cap(binding('keybind_back','Tab')),(190,12))
preview.save(work/'keys-preview.png')
print(json.dumps({k:v for k,v in result.items() if k!='files'}))