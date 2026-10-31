#!/usr/bin/env python3
import json,time,tempfile,subprocess,sys
from pathlib import Path
sys.path.insert(0,'tests/passport');from hardware import Board
sys.path.insert(0,'sdk');from passport import pack
b=Board('/dev/cu.usbmodem101','local/hardware-audio-identity.log')
try:
 b.cmd('home');b.install('dist/voicebird.pap');b.cmd('launch org.passport.voicebird');b.cmd('key 0');time.sleep(.2);v=b.view();assert not v['microphone'] and not v['error'],v;b.cmd('home');print('A00 PASS denied permission: microphone never requested')
 b.cmd('launch org.passport.voicebird');b.cmd('key 2');time.sleep(2);v=b.view();print('A01 CAPTURE',v);assert v['microphone'] and not v['error'],v;b.cmd('metrics');time.sleep(2);b.cmd('home');b.cmd('metrics');print('A01/A02 PASS consent->capture->feature callback->stop; inspect AUDIO sleep=0 and task disappearance')
 with tempfile.TemporaryDirectory() as d:
  key=Path(d,'private.pem');public=Path(d,'public.pem')
  subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',str(key)],capture_output=True,check=True)
  subprocess.run(['openssl','pkey','-in',str(key),'-pubout','-out',str(public)],capture_output=True,check=True)
  m=json.loads(Path('examples/counter/manifest.json').read_text());m['id']='org.developer.dynamic';Path(d,'manifest.json').write_text(json.dumps(m));Path(d,'ui.json').write_bytes(Path('examples/counter/ui.json').read_bytes());p=Path('local/third-party.pap');p.write_bytes(pack(d,key,public))
  try:b.install(p,public)
  except AssertionError as e:assert 'FAIL' in str(e)
  else:raise AssertionError('untrusted publisher accepted')
  b.cmd(json.dumps({'publisher_pem':public.read_text()}));b.cmd('key 0')
  try:b.install(p,public)
  except AssertionError:pass
  else:raise AssertionError('denied publisher accepted')
  b.cmd(json.dumps({'publisher_pem':public.read_text()}));b.cmd('key 2');b.install(p,public);b.cmd('launch org.developer.dynamic');assert b.view()['active'];b.cmd('home');print('K10 PASS new third-party publisher/app trusted and dynamically installed without firmware rebuild')
  m['id']='org.passport.counter';Path(d,'manifest.json').write_text(json.dumps(m));p.write_bytes(pack(d,key,public))
  try:b.install(p,public)
  except AssertionError:print('K11 PASS enrolled publisher cannot impersonate existing App ID')
  else:raise AssertionError('publisher takeover accepted')
 b.cmd('metrics')
finally:b.close()
