#!/usr/bin/env python3
"""Run only on an explicitly authorized Passport; never flashes or erases."""
import argparse,json,time,sys
from pathlib import Path
import serial
sys.path.insert(0,'sdk')
from passport import unpack
class Board:
 def __init__(self,port,log):self.s=serial.Serial(port,115200,timeout=.1);self.log=open(log,'wb')
 def read_until(self,prefix,timeout=15):
  end=time.monotonic()+timeout
  while time.monotonic()<end:
   line=self.s.readline()
   if not line:continue
   self.log.write(line);self.log.flush();text=line.decode(errors='replace').strip()
   if 'panic' in text.lower() or 'Guru Meditation' in text:raise AssertionError(text)
   if prefix=='PASSPORT READY ' and text.startswith('PASSPORT install FAIL'):raise AssertionError(text)
   if text.startswith(prefix):return text
  raise TimeoutError(prefix)
 def cmd(self,line,timeout=15):self.s.write((line+'\n').encode());return self.read_until('PASSPORT DONE',timeout)
 def view(self):
  self.s.write(b'view\n');line=self.read_until('PASSPORT VIEW ');self.read_until('PASSPORT DONE');return json.loads(line[len('PASSPORT VIEW '):])
 def install(self,path,public="sdk/publisher-public.pem"):
  data=Path(path).read_bytes();unpack(data,public);self.s.write(f'install {len(data)}\n'.encode());self.read_until('PASSPORT READY ')
  for i in range(0,len(data),128):
   block=data[i:i+128];self.s.write(block);self.s.flush();self.read_until('PASSPORT ACK '+str(i+len(block)),3)
  result=self.read_until('PASSPORT install ',20)
  if 'OK' not in result:raise AssertionError(result)
 def close(self):
  try:
   if self.s.is_open:self.s.write(b'cancel_cycle\n');self.s.flush()
  except (OSError,serial.SerialException):pass
  self.s.close();self.log.close()
def main():
 p=argparse.ArgumentParser();p.add_argument('--port',default='/dev/cu.usbmodem101');p.add_argument('--suite',choices=['storage','apps','store','stress','reconnect'],default='storage');a=p.parse_args();b=Board(a.port,f'local/hardware-{a.suite}.log')
 try:
  b.cmd('home')
  if a.suite=='storage':
   b.install('dist/counter.pap');b.cmd('launch org.passport.counter');assert b.view()['active'];b.cmd('key 0');b.cmd('key 0');b.cmd('key 2');before=b.view()['text'];assert not b.view()['error'];b.cmd('home');b.cmd('offload org.passport.counter');b.install('dist/counter.pap');b.cmd('launch org.passport.counter');after=b.view()['text'];assert before.splitlines()[0]==after.splitlines()[0];assert before.splitlines()[0]!='0';b.cmd('home');print('S00/S01 PASS hardware signed install, save, exit, offload, reinstall, restored UI:',after.splitlines()[0])
  elif a.suite=='apps':
   for name in ['weather','tetris','voicebird','ws-chat']:
    b.install(f'dist/{name}.pap');b.cmd(f'launch org.passport.{name}');time.sleep(3)
    if name=='voicebird':b.cmd('key 0') # deny microphone
    v=b.view();assert v['active'] and not v['error'],v
    if name=='tetris':
     for k in [0,1,2,0,2,1]:b.cmd('key '+str(k))
     time.sleep(5);v=b.view();assert not v['error'];assert v['lua_peak']<=32768
    print(name,'hardware runtime PASS',v);b.cmd('metrics');b.cmd('home')
  elif a.suite=='store':
   b.cmd('service 192.168.1.10 8040 8041');b.cmd('key 2');b.cmd('key 2');time.sleep(3);b.cmd('key 2');time.sleep(3);b.cmd('metrics');print('Store requests exercised; inspect log/server access for installation result')
  elif a.suite=='stress':b.cmd('cycle org.passport.counter 500',120);print('500 real loader sessions completed; inspect CYCLE drift in log')
  elif a.suite=='reconnect':b.cmd('reconnect');time.sleep(8);b.cmd('metrics');print('Reconnect requested; inspect WIFI online in log')
 finally:b.close()
if __name__=='__main__':main()
