#!/usr/bin/env python3
import tempfile,json,sys,time
from pathlib import Path
sys.path.insert(0,'tests/passport');from hardware import Board
sys.path.insert(0,'sdk');from passport import pack
b=Board('/dev/cu.usbmodem101','local/hardware-faults.log')
try:
 b.cmd('home')
 cases={
 'loop':'function on_tick(dt) while true do end end',
 'oom':'function on_tick(dt) local t={} for i=1,100000 do t[i]=i end end',
 'unsafe':'assert(io==nil and os==nil and debug==nil and package==nil and pcall==nil); function on_start() passport.text("sandbox OK") end',
 'draw':'function on_tick(dt) passport.rect(2147483647,0,2147483647,1,0) end',
 'storage':'function on_tick(dt) passport.set("x","1") end',
 'throw':'function on_tick(dt) error("fixture crash") end'
 }
 for name,source in cases.items():
  with tempfile.TemporaryDirectory() as d:
   m=json.loads(Path('examples/ws-chat/manifest.json').read_text());m.update(id='org.passport.faultprobe',permissions=['ui','graphics','input','timer'])
   Path(d,'manifest.json').write_text(json.dumps(m));Path(d,'main.lua').write_text(source);p=Path('local/faultprobe.pap');p.write_bytes(pack(d,'local/publisher-private.pem','sdk/publisher-public.pem'));b.install(p);b.cmd('launch org.passport.faultprobe');time.sleep(.3);view=b.view()
   if name=='unsafe':assert view['active'] and view['text']=='sandbox OK',view
   else:assert not view['active'] and view['error'],view
   b.cmd('home');print(name,'PASS hardware',view['error'])
 b.cmd('remove org.passport.faultprobe');b.cmd('metrics');b.cmd('launch org.passport.counter');assert not b.view()['error'];b.cmd('home');print('K12 PASS fresh known app runs after all fault cases')
finally:b.close()
