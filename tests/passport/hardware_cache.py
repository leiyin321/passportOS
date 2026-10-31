#!/usr/bin/env python3
import json,tempfile,sys,time
from pathlib import Path
sys.path.insert(0,'tests/passport');from hardware import Board
sys.path.insert(0,'sdk');from passport import pack
b=Board('/dev/cu.usbmodem101','local/hardware-cache.log')
try:
 b.cmd('home');b.cmd('remove org.passport.cachefill')
 for name in ('counter','weather','tetris','voicebird','ws-chat'):b.install(f'dist/{name}.pap')
 b.cmd('launch org.passport.tetris');b.cmd('key 0');time.sleep(1);before=b.view();b.cmd('home');b.s.write(b'data_hash org.passport.tetris game\n');saved=b.read_until('PASSPORT DATA_HASH ');b.read_until('PASSPORT DONE')
 b.cmd('launch org.passport.counter');b.cmd('key 0');b.cmd('key 2');counter=b.view()['text'].splitlines()[0];b.cmd('home');b.cmd('pin org.passport.counter')
 b.cmd('cache_budget 21000')
 with tempfile.TemporaryDirectory() as d:
  m=json.loads(Path('examples/ws-chat/manifest.json').read_text());m.update(id='org.passport.cachefill',permissions=['ui']);Path(d,'manifest.json').write_text(json.dumps(m));Path(d,'main.lua').write_text('--'+('x'*15998));p=Path('local/cachefill.pap');p.write_bytes(pack(d,'local/publisher-private.pem','sdk/publisher-public.pem'));b.install(p)
 b.cmd('launch org.passport.tetris');assert not b.view()['active'];b.cmd('home')
 b.cmd('launch org.passport.counter');assert b.view()['active'] and b.view()['text'].splitlines()[0]==counter;b.cmd('home');print('S04 PASS pinned counter survives automatic cache eviction with data intact')
 b.cmd('cache_budget 0');b.install('dist/tetris.pap');b.s.write(b'data_hash org.passport.tetris game\n');restored=b.read_until('PASSPORT DATA_HASH ');b.read_until('PASSPORT DONE');assert saved==restored and 'FAIL' not in saved;b.cmd('launch org.passport.tetris');after=b.view();assert not after['error'];assert before['text'].splitlines()[0]==after['text'].splitlines()[0];b.cmd('home');print('S05 PASS automatically offloaded Tetris reinstall restores score/progress')
 b.cmd('remove org.passport.cachefill');b.cmd('metrics')
finally:
 try:b.cmd('cache_budget 0')
 finally:b.close()
