import argparse,hashlib,json,sys
from pathlib import Path
from . import pack,unpack
from .files import atomic_write
def main():
 p=argparse.ArgumentParser(prog='passport');sub=p.add_subparsers(dest='command',required=True)
 a=sub.add_parser('pack');a.add_argument('directory');a.add_argument('--key',required=True);a.add_argument('--public',required=True);a.add_argument('-o','--output',required=True)
 a=sub.add_parser('validate');a.add_argument('package');a.add_argument('--public',required=True)
 a=sub.add_parser('publish-index');a.add_argument('directory');a.add_argument('--public',required=True,action='append',help='publisher public key; repeat once for each catalog publisher');a.add_argument('--base-url',default='');a.add_argument('-o','--output',required=True)
 a=sub.add_parser('serve');a.add_argument('directory');a.add_argument('--host',default='127.0.0.1',help='bind address (default: localhost; use 0.0.0.0 only for a trusted development LAN)');a.add_argument('--port',type=int,default=8040)
 a=sub.add_parser('wifi');a.add_argument('--port',default='/dev/cu.usbmodem101');a.add_argument('--ssid',required=True)
 a=sub.add_parser('install');a.add_argument('package');a.add_argument('--port',default='/dev/cu.usbmodem101');a.add_argument('--public',required=True)
 a=sub.add_parser('device');a.add_argument('operation',choices=['list','metrics','home','launch','offload','pin','remove','view','reconnect','service','catalog','update-source','screen','icons','cancel_cycle','data_hash','cycle','cycle-render','button','display-metrics','font-probe']);a.add_argument('arguments',nargs='*');a.add_argument('--port',default='/dev/cu.usbmodem101');a.add_argument('--verbose',action='store_true',help='show all serial log lines');a.add_argument('--observe',type=float,default=0,help='keep the same port open for 0..60 seconds after completion');a.add_argument('--timeout',type=float,default=8,help='completion timeout in seconds (1..600)')
 a=sub.add_parser('trust');a.add_argument('--public',required=True);a.add_argument('--port',default='/dev/cu.usbmodem101')
 a=sub.add_parser('api')
 a=sub.add_parser('init');a.add_argument('directory');a.add_argument('--id',required=True);a.add_argument('--runtime',choices=['lua','declarative','native-elf'],default='native-elf')
 a=sub.add_parser('native');a.add_argument('operation',choices=['build','inspect']);a.add_argument('path')
 a=sub.add_parser('keygen');a.add_argument('--private',required=True);a.add_argument('--public',required=True)
 a=sub.add_parser('firmware-release');a.add_argument('image');a.add_argument('--version',required=True);a.add_argument('--build',required=True,type=int);a.add_argument('--key',required=True);a.add_argument('--public',default='sdk/publisher-public.pem');a.add_argument('-o','--output',required=True)
 a=sub.add_parser('assets');asset_sub=a.add_subparsers(dest='asset_command',required=True)
 a=asset_sub.add_parser('build');a.add_argument('directory');a.add_argument('--manifest');a.add_argument('--output');a.add_argument('--no-dither',action='store_true')
 for action in ('inspect','report'):
  a=asset_sub.add_parser(action);a.add_argument('file');a.add_argument('--public');a.add_argument('--preview')
 args=p.parse_args()
 try:
  if args.command=='assets':
   from .assets import build_manifest_assets,validate_blob,decode_blob
   if args.asset_command=='build':
    manifest=Path(args.manifest) if args.manifest else Path(args.directory)/'manifest.json';m=json.loads(manifest.read_text());records,blob=build_manifest_assets(manifest.parent,m.get('assets',[]));destination=Path(args.output) if args.output else manifest.parent/'.passport-assets';destination.mkdir(parents=True,exist_ok=True)
    for r in records:
     target=destination/(r['id']+'.pai');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob[r['offset']:r['offset']+r['size']])
    (destination/'index.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps({'lvgl':'9.5','assets':records,'bytes':len(blob)},indent=2))
   else:
    data=Path(args.file).read_bytes()
    if data[:4]==b'PAI1':
     print(json.dumps(validate_blob(data),indent=2))
     if args.preview:decode_blob(data).save(args.preview)
    else:
     if not args.public:raise ValueError('--public is required for package verification')
     from . import unpack_assets
     m,code,assets=unpack_assets(data,args.public);rows=[]
     for rid,blob in assets.items():
      info=validate_blob(blob);baseline=info['width']*info['height']*2+24;rows.append({'id':rid,**info,'rgb565_baseline':baseline,'saved_percent':round((1-info['size']/baseline)*100,2)})
     total=sum(r['size'] for r in rows);baseline=sum(r['rgb565_baseline'] for r in rows);print(json.dumps({'package_bytes':len(data),'code_bytes':len(code),'asset_count':len(rows),'asset_bytes':total,'rgb565_baseline_bytes':baseline,'asset_saved_percent':round((1-total/baseline)*100,2) if baseline else 0,'assets':rows},indent=2))
  elif args.command=='api':
   from importlib.resources import files
   print(files('passport').joinpath('api-v1.json').read_text())
  elif args.command=='keygen':
   import subprocess
   private,public=Path(args.private),Path(args.public)
   if private.exists() or public.exists():raise ValueError('refusing to overwrite key files')
   private.parent.mkdir(parents=True,exist_ok=True);public.parent.mkdir(parents=True,exist_ok=True)
   subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',str(private)],check=True,capture_output=True);private.chmod(0o600)
   subprocess.run(['openssl','pkey','-in',str(private),'-pubout','-out',str(public)],check=True,capture_output=True);print(public)
  elif args.command=='native':
   from .native import build,validate_elf
   result=build(args.path)[1] if args.operation=='build' else validate_elf(Path(args.path).read_bytes())
   print(json.dumps(result,indent=2))
  elif args.command=='init':
   from . import validate_manifest
   d=Path(args.directory)
   if d.exists() and any(d.iterdir()):raise ValueError('directory must be empty')
   d.mkdir(parents=True,exist_ok=True)
   m=dict(format='passport.app/1',id=args.id,name='My Passport App',version='1.0.0',runtime=args.runtime,entry='main.lua' if args.runtime=='lua' else 'ui.json',min_os_api=1,data_schema=1,permissions=['ui','input','timer','storage.private'],limits=dict(script_heap_kib=32,fps=20,persistent_kib=4))
   if args.runtime=='native-elf':m.update(entry='app.app.elf',target='esp32c3',abi=1,trusted=True,permissions=['ui','graphics','input','timer'],limits=dict(native_exec_kib=16,fps=10,persistent_kib=1))
   validate_manifest(m)
   (d/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
   code="local count=0\nfunction on_start() count=tonumber(passport.get('count')) or 0 passport.text('Count '..count) end\nfunction on_button(b) count=count+(b==1 and -1 or 1) passport.set('count',tostring(count)) passport.text('Count '..count) end\n"
   if args.runtime=='declarative':code=json.dumps({'format':'passport.ui/1','state':{'count':0},'ui':[{'type':'text','bind':'count'}],'actions':{'up':{'type':'increment','field':'count'},'down':{'type':'increment','field':'count','amount':-1},'ok':{'type':'save'}}},indent=2)+'\n'
   if args.runtime=='native-elf':
    from .native import sdk_root
    (d/'main.c').write_bytes((sdk_root()/'template/main.c').read_bytes())
    (d/'.gitignore').write_text('/app.app.elf\n/*.pap\n')
   else:(d/m['entry']).write_text(code)
   print(d)
  elif args.command=='pack':atomic_write(args.output,pack(args.directory,args.key,args.public));print(args.output)
  elif args.command=='validate':m,_=unpack(Path(args.package).read_bytes(),args.public);print(json.dumps(m,indent=2))
  elif args.command=='firmware-release':
   from . import key_id
   from .update import publish_firmware
   manifest=publish_firmware(args.image,args.version,args.build,args.key,args.public,args.output)
   print(json.dumps({'manifest':str(Path(args.output)/'latest.json'),'image':manifest['image'],'size':manifest['size'],'sha256':manifest['sha256'],'build':manifest['build']},indent=2))
  elif args.command=='publish-index':
   apps=[];app_ids=set()
   for f in sorted(Path(args.directory).glob('*.pap')):
    if len(apps)>=8:raise ValueError('device catalog v1 supports at most 8 apps')
    data=f.read_bytes();m=None;last_error=None
    for public in args.public:
     try:m,_=unpack(data,public);break
     except ValueError as error:last_error=error
    if m is None:raise ValueError(f'{f.name}: no supplied publisher key verified the package ({last_error})')
    if m['id'] in app_ids:raise ValueError(f'duplicate app ID in catalog: {m["id"]}')
    app_ids.add(m['id'])
    from urllib.parse import quote
    filename=quote(f.name,safe='-._~')
    apps.append({**{k:m[k] for k in ['id','version','publisher_key_id','permissions']},'icon':m.get('icon','system.app-default'),'package_format':2 if data[:4]==b'PAP2' else 1,'features':(['assets/1'] if m.get('assets') else [])+(['native-elf/'+str(m['abi'])] if m['runtime']=='native-elf' else []),'name':m['name'],'url':(args.base_url.rstrip('/')+'/' if args.base_url else '')+filename,'sha256':hashlib.sha256(data).hexdigest(),'size':len(data)})
   atomic_write(args.output,(json.dumps({'format':'passport.catalog/1','apps':apps},indent=2)+'\n').encode())
  elif args.command=='serve':
   import functools,http.server
   http.server.ThreadingHTTPServer((args.host,args.port),functools.partial(http.server.SimpleHTTPRequestHandler,directory=args.directory)).serve_forever()
  elif args.command in ('install','device'):
   import serial,time
   from .device import read_completion
   if args.command == 'device':
    if not 0 <= args.observe <= 60:raise ValueError('observe must be between 0 and 60 seconds')
    if any('\r' in arg or '\n' in arg or '\0' in arg for arg in args.arguments):raise ValueError('invalid device command argument')
   timeout=getattr(args,'timeout',8)
   if not 1<=timeout<=600:raise ValueError('timeout must be in 1..600 seconds')
   with serial.Serial(args.port,115200,timeout=.2) as port:
    port.reset_input_buffer()
    if args.command=='install':
     data=Path(args.package).read_bytes();unpack(data,args.public)
     def await_line(prefix,seconds=20):
      deadline=time.monotonic()+seconds
      while time.monotonic()<deadline:
       line=port.readline().decode(errors='replace').strip()
       if line.startswith('PASSPORT install FAIL'):raise ValueError(line)
       if line.startswith(prefix):return line
      raise ValueError('device protocol timeout: '+prefix)
     port.write(('install '+str(len(data))+'\n').encode())
     await_line('PASSPORT READY ')
     for offset in range(0,len(data),128):
      block=data[offset:offset+128];port.write(block);port.flush()
      await_line('PASSPORT ACK '+str(offset+len(block)))
    else:
     if args.operation=='remove':
      if input('Delete package AND all private data? Type DELETE: ')!='DELETE':raise ValueError('cancelled')
     operation=args.operation.replace('-','_')
     port.write((operation+' '+ ' '.join(args.arguments)+'\n').strip().encode()+b'\n')
    read_completion(port, timeout=timeout, observe=getattr(args,'observe',0), verbose=getattr(args,'verbose',False))
  elif args.command=='trust':
   import serial,time
   from . import key_id
   public=Path(args.public).read_text();print('Publisher key ID:',key_id(args.public))
   with serial.Serial(args.port,115200,timeout=1) as port:port.write((json.dumps({'publisher_pem':public})+'\n').encode());time.sleep(.3)
   print('Confirm the displayed key on the device. Installation requires that confirmation.')
  elif args.command=='wifi':
   import getpass,serial,time
   pw=getpass.getpass('Wi-Fi password (never logged): ')
   with serial.Serial(args.port,115200,timeout=1) as s:
    s.write((json.dumps({'ssid':args.ssid,'password':pw})+'\n').encode());time.sleep(1);print('Configuration sent; check device network status.')
 except (ValueError,OSError) as e:print('passport:',e,file=sys.stderr);sys.exit(1)
