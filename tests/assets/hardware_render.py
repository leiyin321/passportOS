#!/usr/bin/env python3
import sys,time,hashlib,re,argparse
from pathlib import Path
sys.path.insert(0,'tests/passport');from hardware import Board
sys.path.insert(0,'sdk');from passport import unpack_assets
from passport.assets import decode_blob
parser=argparse.ArgumentParser();parser.add_argument('--cycles',type=int,default=500);parser.add_argument('--scrolls',type=int,default=500);args=parser.parse_args()
b=Board('/dev/cu.usbmodem101','local/hardware-render.log')
try:
 b.cmd('home');b.s.write(b'icons\n');line=b.read_until('PASSPORT ICONS');b.read_until('PASSPORT DONE');assert 'fonts=1' in line,line;print(line,flush=True)
 for name,identity,uri in [('weather','weather','icons/weather'),('icon_gallery','icon-gallery','masks/test')]:
  b.install(f'dist/{name}.pap');b.cmd(f'launch org.passport.{identity}')
  _,_,assets=unpack_assets(Path(f'dist/{name}.pap').read_bytes(),'sdk/publisher-public.pem');im=decode_blob(assets[uri]);raw=im.getchannel('A').tobytes() if name=='icon_gallery' else im.tobytes('raw','BGRA');expected=hashlib.sha256(raw).hexdigest()
  b.s.write(f'asset_probe app://{uri}\n'.encode());line=b.read_until('PASSPORT DECODE');b.read_until('PASSPORT DONE');assert 'ok=1' in line and expected in line,(line,expected);print(name,'LVGL pixels/alpha PASS',line,flush=True);b.cmd('home')
 b.install('dist/appstore_grid.pap');b.cmd('launch org.passport.appstore-grid');assert b.view()['active'];b.cmd('metrics')
 for i in range(args.scrolls):
  b.cmd('key 1')
  if i%100==99:
   v=b.view();assert v['active'] and not v['error'] and v['asset_bytes']<=16384,v;print('scroll',i+1,'PASS cache',v['asset_bytes'],'peak',v['asset_peak'],flush=True)
 b.cmd('metrics');b.cmd('home');assert b.view()['asset_bytes']==0
 b.cmd(f'cycle_render org.passport.appstore-grid {args.cycles}',max(180,args.cycles));print(f'{args.cycles} real grid load/render/exit cycles completed',flush=True);b.cmd('metrics')
finally:b.close()
