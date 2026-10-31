#!/usr/bin/env python3
"""Loopback browser simulator using the actual C runtime, never shell eval."""
import argparse,json,subprocess,tempfile,threading,sys,hashlib
from pathlib import Path
from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'sdk'))
from passport import unpack_assets
ROOT=Path(__file__).resolve().parents[1]
lock=threading.Lock();process=None;workspace=None;active_assets={}
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def output(self,value,status=200):
  raw=json.dumps(value).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
 def do_GET(self):
  if self.path.startswith('/asset/'):
   from urllib.parse import unquote
   import io
   from passport.assets import decode_blob,resource_id
   try:
    rid=resource_id(unquote(self.path[7:]));data=active_assets[rid];output=io.BytesIO();decode_blob(data).save(output,format='PNG');raw=output.getvalue();self.send_response(200);self.send_header('Content-Type','image/png');self.end_headers();self.wfile.write(raw);return
   except (ValueError,KeyError):return self.output({'error':'asset missing'},404)
  if self.path=='/apps':return self.output({'apps':[p.stem for p in (ROOT/'dist').glob('*.pap')]})
  if self.path!='/':return self.output({'error':'not found'},404)
  raw=(ROOT/'simulator/index.html').read_bytes();self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.end_headers();self.wfile.write(raw)
 def do_POST(self):
  global process,workspace,active_assets
  if self.headers.get('Origin') not in (None,f'http://127.0.0.1:{self.server.server_port}',f'http://localhost:{self.server.server_port}'):return self.output({'error':'origin denied'},403)
  n=int(self.headers.get('Content-Length','0'))
  if n>2048:return self.output({'error':'too large'},413)
  try:
   obj=json.loads(self.rfile.read(n) or b'{}')
   with lock:
    if self.path in ('/launch','/stop'):
     if process:process.communicate('stop\n',timeout=3);process=None
     if workspace:workspace.cleanup();workspace=None
    if self.path=='/launch':
     name=obj.get('app','')
     if name not in [p.stem for p in (ROOT/'dist').glob('*.pap')]:raise ValueError('unknown app')
     package=(ROOT/'dist'/f'{name}.pap').read_bytes();m,payload,active_assets=unpack_assets(package,ROOT/'sdk/publisher-public.pem');workspace=tempfile.TemporaryDirectory();tmp=Path(workspace.name)
     (tmp/'manifest.json').write_text(json.dumps(m));(tmp/m['entry']).write_bytes(payload);(tmp/'package.pap').write_bytes(package)
     identity=hashlib.sha256((m['publisher_key_id']+m['id']).encode()).hexdigest()[:24];data=ROOT/'local/simulator'/identity;data.mkdir(parents=True,exist_ok=True)
     process=subprocess.Popen([str(ROOT/'build/simulator/runtime'),str(tmp/'manifest.json'),str(tmp/m['entry']),str(data)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
     return self.output(json.loads(process.stdout.readline()))
    if self.path=='/event' and process:
     if obj.get('name') not in ('on_tick','on_button','on_audio','permission'):raise ValueError('event denied')
     value=int(obj.get('value',0))
     if not -1000<=value<=1000:raise ValueError('value bounds')
     process.stdin.write(f'{obj["name"]} {value}\n');process.stdin.flush();return self.output(json.loads(process.stdout.readline()))
    return self.output({'text':'Stopped'})
  except (ValueError,KeyError,OSError,subprocess.SubprocessError) as e:return self.output({'error':str(e)},400)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--port',type=int,default=8050);a=p.parse_args();print(f'Simulator http://127.0.0.1:{a.port}',flush=True);ThreadingHTTPServer(('127.0.0.1',a.port),Handler).serve_forever()
