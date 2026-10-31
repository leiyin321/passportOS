#!/usr/bin/env python3
"""Owned-service network tests. Run after approving hardware operations."""
import json,time,sys,tempfile
from pathlib import Path
sys.path.insert(0,'tests/passport');from hardware import Board
sys.path.insert(0,'sdk');from passport import pack
b=Board('/dev/cu.usbmodem101','local/hardware-network.log')
try:
 b.cmd('home');b.cmd(json.dumps({'time':int(time.time())}))
 b.cmd('launch org.passport.weather')
 end=time.monotonic()+14;weather={}
 while time.monotonic()<end:
  time.sleep(1);weather=b.view()
  if 'Loading...' not in weather['text']:break
 print('WEATHER',weather)
 b.cmd('metrics');b.cmd('home')
 b.cmd('service 192.168.1.10 8040 8041');b.cmd('key 2');b.cmd('launch org.passport.ws-chat');time.sleep(2);b.cmd('key 2');time.sleep(.4);text=b.view();print('WS_TEXT',text)
 assert 'Passport ping' in text['text'] and 'Channel text' in text['text'],text
 b.cmd('key 0');time.sleep(.4);binary=b.view();print('WS_BINARY',binary);assert 'binary' in binary['text'],binary
 b.cmd('metrics');b.cmd('home')
 # Every HTTP redirect reuses the same policy entrypoint and pinned resolver.
 b.cmd('service 192.168.1.10 8042 8041');b.cmd('key 2')
 for route,expected in [('redirect-good','200'),('redirect-bad','-1'),('redirect-port','-1'),('redirect-loop','-1'),('oversize','-1')]:
  with tempfile.TemporaryDirectory() as d:
   m=json.loads(Path('examples/ws-chat/manifest.json').read_text());m.update(id='org.passport.netprobe',permissions=['ui','network.fetch'])
   Path(d,'manifest.json').write_text(json.dumps(m));Path(d,'main.lua').write_text("function on_start() passport.fetch('http://192.168.1.10:8042/"+route+"') passport.text('waiting') end\nfunction on_http(status,body) passport.text(tostring(status)..' '..body) end\n")
   p=Path('local/netprobe.pap');p.write_bytes(pack(d,'local/publisher-private.pem','sdk/publisher-public.pem'));b.install(p);b.cmd('launch org.passport.netprobe')
   end=time.monotonic()+10;v={}
   while time.monotonic()<end:
    time.sleep(.2);v=b.view()
    if v['text']!='waiting':break
   assert v['text'].startswith(expected),v
   print('REDIRECT',route,'PASS',v['text']);b.cmd('home')
 b.cmd('service 192.168.1.10 8040 8041');b.cmd('key 2');b.cmd('metrics')
finally:b.close()
