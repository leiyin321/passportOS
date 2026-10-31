#!/usr/bin/env python3
"""Bounded serial recorder. Does not reset board or log outgoing credentials."""
import argparse,time,serial
p=argparse.ArgumentParser();p.add_argument('--seconds',type=float,default=20);p.add_argument('--port',default='/dev/cu.usbmodem101');p.add_argument('--send',action='append',default=[]);p.add_argument('--output',required=True);a=p.parse_args()
with serial.Serial(a.port,115200,timeout=.2) as s,open(a.output,'wb') as f:
 for cmd in a.send:s.write((cmd+'\n').encode())
 end=time.monotonic()+a.seconds
 while time.monotonic()<end:
  data=s.read(s.in_waiting or 1)
  if data:f.write(data);f.flush()
