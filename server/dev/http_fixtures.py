#!/usr/bin/env python3
"""Static catalog plus deterministic redirect/security fixtures."""
import argparse,functools,json
from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
class Fixture(SimpleHTTPRequestHandler):
 def do_GET(self):
  targets={'/redirect-good':'/probe.json','/redirect-bad':'http://127.0.0.1:8042/probe.json','/redirect-loop':'/redirect-loop','/redirect-port':f'http://{self.headers.get("Host", "127.0.0.1").split(":")[0]}:1/probe.json'}
  if self.path in targets:
   self.send_response(302);self.send_header('Location',targets[self.path]);self.send_header('Content-Length','0');self.end_headers();return
  if self.path=='/probe.json':
   data=b'{"ok":true}';self.send_response(200);self.send_header('Content-Length',str(len(data)));self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(data);return
  if self.path=='/oversize':
   data=b'x'*25000;self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
  super().do_GET()
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--port',type=int,default=8042);p.add_argument('--directory',default='dist');a=p.parse_args();print(f'HTTP fixtures ready {a.port}',flush=True);ThreadingHTTPServer(('0.0.0.0',a.port),functools.partial(Fixture,directory=a.directory)).serve_forever()
