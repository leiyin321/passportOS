#!/usr/bin/env python3
"""Explicit developer rollback test; writes the inactive slot, never appdata.
Use only with user authorization, a complete original Flash backup and matching image.
This is not an enabled network OTA updater or a firmware signature-verification claim.
"""
import sys,time,subprocess,hashlib,json,argparse
from pathlib import Path
sys.path.insert(0,'tests/passport');from hardware import Board
PORT='/dev/cu.usbmodem101'
parser=argparse.ArgumentParser();parser.add_argument('--image',type=Path,default=Path('build-v1/FoloToy-AI-Passport.bin'));args=parser.parse_args()
image=args.image;print('Test image SHA256',hashlib.sha256(image.read_bytes()).hexdigest(),flush=True)
b=Board(PORT,'local/ota-baseline.log');b.s.write(b'ota_status\n');current=b.read_until('PASSPORT OTA running=');b.read_until('PASSPORT DONE');assert 'ota_0' in current,current;b.s.write(b'ota_prepare\n');print(b.read_until('PASSPORT OTA select='),flush=True);b.read_until('I ',2);b.close();time.sleep(4)
# Both slots contain the exact same verified build; tests validate boot states/selection.
subprocess.run([sys.executable,'-m','esptool','--chip','esp32c3','-p',PORT,'-b','460800','write_flash','0x2a0000',str(image)],stdout=open('local/ota-inactive-flash.log','w'),stderr=subprocess.STDOUT,check=True)
time.sleep(4)
for mode in ('reject','reset','accept','accept'):
 b=Board(PORT,f'local/ota-{mode}-{time.time_ns()}.log')
 try:
  b.s.write(('ota_trial '+mode+'\n').encode());print(b.read_until('PASSPORT OTA select='),flush=True)
  if mode=='reset':
   deadline=time.monotonic()+15
   while time.monotonic()<deadline:
    line=b.s.readline();b.log.write(line)
    if b'PENDING reset-test window' in line:break
   else:raise TimeoutError('pending reset-test window')
   b.close();b=None
   subprocess.run([sys.executable,'-m','esptool','--chip','esp32c3','-p',PORT,'run'],stdout=open('local/ota-reset.log','w'),stderr=subprocess.STDOUT,check=True)
  else:
   end=time.monotonic()+6
   while time.monotonic()<end:
    line=b.s.readline();b.log.write(line)
 finally:
  if b:b.close()
 time.sleep(3)
 b=Board(PORT,'local/ota-state-'+mode+str(time.time_ns())+'.log')
 try:
  b.s.write(b'ota_status\n');state=b.read_until('PASSPORT OTA running=');b.read_until('PASSPORT DONE');print(mode,state,flush=True)
  if mode in ('reject','reset'):assert 'ota_0' in state and 'state=2' in state,state
 finally:b.close()
print('Rollback/restart/accept tests complete. Network OTA stays disabled.',flush=True)
