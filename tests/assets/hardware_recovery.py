#!/usr/bin/env python3
import sys,time,subprocess,json
from pathlib import Path
sys.path.insert(0,'tests/passport');from hardware import Board
PORT='/dev/cu.usbmodem101'
def reset_board():
 r=subprocess.run([sys.executable,'-m','esptool','--chip','esp32c3','-p',PORT,'run'],capture_output=True,text=True,check=True);Path('local/i6-reset.log').write_text(r.stdout+r.stderr);time.sleep(4)
def digest(board):
 board.s.write(b'data_hash org.passport.appstore-grid page\n');v=board.read_until('PASSPORT DATA_HASH ');board.read_until('PASSPORT DONE');assert 'FAIL' not in v,v;return v
b=Board(PORT,'local/hardware-recovery.log')
try:
 b.cmd('home');b.install('dist/appstore_grid.pap');b.cmd('launch org.passport.appstore-grid');b.cmd('key 1');b.cmd('key 1');b.cmd('home');saved=digest(b)
 b.cmd('offload org.passport.appstore-grid');assert digest(b)==saved;b.install('dist/appstore_grid.pap');assert digest(b)==saved;b.cmd('launch org.passport.appstore-grid');v=b.view();assert not v['error'] and v['images']==6;b.cmd('home');print('I6 offload/reinstall preserves saved page and asset drawing PASS',flush=True)
 for mode in ('partial','old-only','new-only'):
  b.install('dist/appstore_grid.pap');b.cmd('package_fault org.passport.appstore-grid '+mode);b.cmd('launch org.passport.appstore-grid');v=b.view();assert v['active'] and not v['error'] and v['asset_bytes']>0,v;b.cmd('home');assert digest(b)==saved;print('I6 update boundary',mode,'PASS',flush=True)
 b.install('dist/appstore_grid.pap');b.cmd('package_fault org.passport.appstore-grid old-only');b.close();b=None;reset_board();b=Board(PORT,'local/hardware-recovery-reboot.log');b.cmd('launch org.passport.appstore-grid');assert not b.view()['error'];b.cmd('home');assert digest(b)==saved;print('I6 reboot with old valid + truncated staging PASS',flush=True)
 # Interrupt a real SPIFFS upload at a known acknowledged boundary.
 b.install('dist/appstore_grid.pap');data=Path('dist/appstore_grid.pap').read_bytes();b.s.write(f'install {len(data)}\n'.encode());b.read_until('PASSPORT READY ')
 for offset in range(0,1024,128):b.s.write(data[offset:offset+128]);b.s.flush();b.read_until('PASSPORT ACK '+str(offset+128))
 b.s.close();b.log.close();b=None;reset_board();b=Board(PORT,'local/hardware-recovery-upload-reset.log');b.cmd('launch org.passport.appstore-grid');v=b.view();assert v['active'] and not v['error'] and v['asset_bytes']>0,v;b.cmd('home');assert digest(b)==saved;print('I6 actual reset after 1024B of incomplete upload PASS',flush=True)
 b.install('dist/appstore_grid.pap');b.cmd('metrics')
finally:
 if b:b.close()
