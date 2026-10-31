#!/usr/bin/env python3
import sys,time,json
from pathlib import Path
sys.path.insert(0,'tests/passport');from hardware import Board
b=Board('/dev/cu.usbmodem101','local/hardware-assets.log')
try:
 b.cmd('home');b.cmd('remove org.passport.cachefill')
 for name,identity in [('icon_gallery','icon-gallery'),('appstore_grid','appstore-grid')]:
  b.install(f'dist/{name}.pap');b.cmd(f'launch org.passport.{identity}');time.sleep(.3);v=b.view();print(name,'START',v,flush=True);assert v['active'] and not v['error'],v
  assert v['images']>=6,v
  for i in range(10):b.cmd('key '+str(i%2));time.sleep(.03)
  v=b.view();assert not v['error'];assert v['asset_peak']<=16384;print(name,'SCROLLED',v,flush=True);b.cmd('metrics');b.cmd('home');v=b.view();assert v['asset_bytes']==0,v;b.cmd('metrics');print(name,'CLOSE assets zero',flush=True)
 b.cmd('launch org.passport.icon-gallery');b.cmd('home');b.cmd('cycle org.passport.icon-gallery 500',150);print('I5 500 gallery loader sessions completed',flush=True)
 b.cmd('metrics')
finally:b.close()
