"""Passport Application Package v1 reference encoder and verifier."""
import hashlib,json,re,struct,subprocess,tempfile
from pathlib import Path
MAX_MANIFEST=4096
MAX_PAYLOAD=16384
MAX_NATIVE_PAYLOAD=32768
MAX_PACKAGE=262144
MAX_ASSET_MANIFEST=12288
PERMISSIONS={'ui','graphics','input','timer','storage.private','network.fetch','network.websocket','audio.features','audio.stream','device.identity','bluetooth.hid','audio.playback','audio.http'}
def strict_json(raw):
    def pairs(items):
        d={}
        for k,v in items:
            if k in d:raise ValueError('duplicate JSON key')
            d[k]=v
        return d
    return json.loads(raw,object_pairs_hook=pairs,parse_constant=lambda _:(_ for _ in ()).throw(ValueError('non-finite')))
def validate_manifest(m):
    if not isinstance(m,dict):raise ValueError('manifest must be an object')
    allowed={'format','id','name','version','publisher_key_id','runtime','entry','min_os_api','data_schema','permissions','domains','limits','icon','assets','display','sha256','target','abi','trusted'}
    if set(m)-allowed:raise ValueError('unknown manifest field')
    if m.get('format')!='passport.app/1' or not isinstance(m.get('id'),str) or not re.fullmatch(r'[a-z][a-z0-9]*(?:[.-][a-z0-9]+)+',m['id']) or len(m['id'].encode())>63:raise ValueError('identity')
    if not isinstance(m.get('name'),str) or not m['name'].strip() or len(m['name'].encode())>40:raise ValueError('name')
    if not isinstance(m.get('version'),str) or not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)',m['version']) or any(int(v)>0xffffffff for v in m['version'].split('.')):raise ValueError('version')
    native=m.get('runtime')=='native-elf'
    if m.get('runtime') not in ('declarative','lua','native-elf') or m.get('entry')!=('app.app.elf' if native else 'main.lua' if m.get('runtime')=='lua' else 'ui.json'):raise ValueError('runtime/entry mismatch')
    if not native and {'target','abi','trusted'}&set(m):raise ValueError('native fields require native runtime')
    if native and (m.get('target')!='esp32c3' or type(m.get('abi')) is not int or m['abi'] not in (1,2) or m.get('trusted') is not True):raise ValueError('native target/ABI/trust contract')
    if type(m.get('min_os_api')) is not int or m['min_os_api']!=1 or type(m.get('data_schema')) is not int or m['data_schema']!=1:raise ValueError('unsupported API/schema')
    if 'publisher_key_id' in m and (not isinstance(m['publisher_key_id'],str) or not re.fullmatch(r'[0-9a-f]{16}',m['publisher_key_id'])):raise ValueError('publisher key ID')
    if 'sha256' in m and (not isinstance(m['sha256'],str) or not re.fullmatch(r'[0-9a-f]{64}',m['sha256'])):raise ValueError('entry SHA256')
    if 'icon' in m and (not isinstance(m['icon'],str) or not m['icon'] or len(m['icon'].encode())>64):raise ValueError('icon')
    display=m.get('display',{})
    if not isinstance(display,dict) or set(display)-{'mode','orientation','navigation','design_width','design_height'} or display.get('mode','standard') not in ('standard','fullscreen') or display.get('orientation','portrait')!='portrait' or display.get('navigation','buttons') not in ('buttons','scroll','select','game'):raise ValueError('display mode/orientation/navigation')
    for key,maximum in [('design_width',240),('design_height',320)]:
        if key in display and (type(display[key]) is not int or not 1<=display[key]<=maximum):raise ValueError('display design dimensions')
    perms=m.get('permissions')
    if not isinstance(perms,list) or any(not isinstance(p,str) for p in perms) or len(perms)!=len(set(perms)) or not set(perms)<=PERMISSIONS:raise ValueError('permissions')
    if 'audio.http' in perms and (not native or m['abi']!=2 or not {'audio.playback','network.fetch'}<=set(perms)):raise ValueError('audio.http requires Native ABI 2 playback/fetch')
    if 'audio.playback' in perms and (not native or m['abi']!=2):raise ValueError('audio.playback requires Native ABI 2')
    if 'bluetooth.hid' in perms and (not native or m['abi']!=2):raise ValueError('bluetooth.hid requires Native ABI 2')
    if native and m['abi']==1 and not set(perms)<={'ui','graphics','input','timer'}:raise ValueError('unsupported native permission')
    limits=m.get('limits')
    if not isinstance(limits,dict) or set(limits)-{'script_heap_kib','fps','persistent_kib','image_cache_kib','native_exec_kib'}:raise ValueError('limits fields')
    if ('script_heap_kib' in limits if native else 'native_exec_kib' in limits):raise ValueError('limits/runtime mismatch')
    for k,maxval in ([('native_exec_kib',16)] if native else [('script_heap_kib',64)])+[('fps',20),('persistent_kib',4)]:
        v=limits.get(k,0)
        if type(v)!=int or not 1<=v<=maxval:raise ValueError('limits')
    domains=m.get('domains',[])
    if not isinstance(domains,list) or any(not isinstance(domain,str) for domain in domains) or len(domains)>4 or len(domains)!=len(set(domains)):raise ValueError('domains')
    for domain in domains:
        if not isinstance(domain,str) or len(domain)>127 or not re.fullmatch(r'[a-z0-9]+(?:[.-][a-z0-9]+)+',domain) or re.fullmatch(r'[0-9.]+',domain):raise ValueError('domain')
    if 'image_cache_kib' in limits and (type(limits['image_cache_kib'])!=int or not 1<=limits['image_cache_kib']<=16):raise ValueError('image cache budget')
    if 'assets' in m:
        assets=m['assets']
        if not isinstance(assets,list) or len(assets)>32:raise ValueError('assets')
        ids=set()
        source_fields={'id','source','width','height','encoding','kind','no_dither'}
        compiled_fields={'id','offset','size','format','width','height','stride','palette_count','sha256'}
        for asset in assets:
            if not isinstance(asset,dict) or not isinstance(asset.get('id'),str) or len(asset['id'])>48 or not re.fullmatch(r'[a-z0-9]+(?:[/_-][a-z0-9]+)*',asset['id']) or asset['id'] in ids:raise ValueError('asset ID')
            ids.add(asset['id'])
            if 'source' in asset:
                if set(asset)-source_fields or not isinstance(asset['source'],str) or not asset['source'] or len(asset['source'])>128 or Path(asset['source']).is_absolute() or '..' in Path(asset['source']).parts:raise ValueError('asset source')
                for key in ('width','height'):
                    if key in asset and (type(asset[key])!=int or not 1<=asset[key]<=96):raise ValueError('asset dimensions')
                if asset.get('encoding','auto') not in ('auto','A1','A4','I4','I8','RGB565') or asset.get('kind','sprite') not in ('sprite','app-icon','mask') or ('no_dither' in asset and type(asset['no_dither']) is not bool):raise ValueError('asset compiler options')
            else:
                if set(asset)!=compiled_fields or type(asset.get('offset')) is not int or asset['offset']<0 or type(asset.get('size')) is not int or not 24<=asset['size']<=16384 or asset.get('format') not in ('A1','A4','I4','I8','RGB565') or not isinstance(asset.get('sha256'),str) or not re.fullmatch(r'[0-9a-f]{64}',asset['sha256']):raise ValueError('compiled asset record')
                for key in ('width','height','stride','palette_count'):
                    if type(asset.get(key)) is not int or asset[key]<0:raise ValueError('compiled asset dimensions')
                if not 1<=asset['width']<=96 or not 1<=asset['height']<=96 or not 1<=asset['stride']<=192 or asset['palette_count']>256:raise ValueError('compiled asset dimensions')
    icon=m.get('icon')
    if icon:
        if icon.startswith('app://'):
            resource=icon[6:]
            if not re.fullmatch(r'[a-z0-9]+(?:[/_-][a-z0-9]+)*',resource) or resource not in {a['id'] for a in m.get('assets',[]) if isinstance(a,dict) and isinstance(a.get('id'),str)}:raise ValueError('icon resource is not declared')
        elif not re.fullmatch(r'system\.[a-z0-9]+(?:[.-][a-z0-9]+)*',icon):raise ValueError('icon ID')
    return m
def _openssl(args,data=None):
    r=subprocess.run(['openssl',*map(str,args)],input=data,capture_output=True)
    if r.returncode:raise ValueError('signature/key verification failed')
    return r.stdout
def key_id(public):return hashlib.sha256(_openssl(['pkey','-pubin','-in',public,'-outform','DER'])).hexdigest()[:16]
def pack(directory,key,public):
    d=Path(directory);m=validate_manifest(strict_json((d/'manifest.json').read_bytes()))
    payload=(d/m['entry']).read_bytes()
    if len(payload)>(MAX_NATIVE_PAYLOAD if m['runtime']=='native-elf' else MAX_PAYLOAD):raise ValueError('payload too large')
    if m['runtime']!='native-elf':
        payload.decode('utf-8')
        if b'\0' in payload:raise ValueError('embedded NUL')
    m['publisher_key_id']=key_id(public);m['sha256']=hashlib.sha256(payload).hexdigest()
    assets=m.get('assets',[])
    if assets:
        from .assets import build_manifest_assets
        records,blob=build_manifest_assets(d,assets);m['assets']=records
        body=struct.pack('<I',len(payload))+payload+blob;magic=b'PAP2';manifest_limit=MAX_ASSET_MANIFEST
    else:body=payload;magic=b'PAP1';manifest_limit=MAX_MANIFEST
    validate_manifest(m)
    validate_entry(m,payload)
    raw=json.dumps(m,sort_keys=True,separators=(',',':'),ensure_ascii=True).encode()
    if len(raw)>manifest_limit:raise ValueError('manifest too large')
    signed=struct.pack('<4sIII',magic,len(raw),len(body),256)+raw+body
    if len(signed)+256>MAX_PACKAGE:raise ValueError('package too large')
    sig=_openssl(['dgst','-sha256','-sign',key],signed)
    if len(sig)!=256:raise ValueError('RSA2048 key required')
    package=signed+sig;unpack(package,public);return package
def unpack(data,public):
    if len(data)<16:raise ValueError('short header')
    magic,ml,pl,sl=struct.unpack_from('<4sIII',data)
    if magic not in (b'PAP1',b'PAP2') or not 0<ml<=(MAX_MANIFEST if magic==b'PAP1' else MAX_ASSET_MANIFEST) or not pl or sl!=256 or len(data)!=16+ml+pl+sl or len(data)>MAX_PACKAGE:raise ValueError('length/format')
    if magic==b'PAP1' and pl>MAX_NATIVE_PAYLOAD:raise ValueError('payload size')
    with tempfile.TemporaryDirectory() as tmp:
        sig=Path(tmp)/'signature';sig.write_bytes(data[-sl:])
        _openssl(['dgst','-sha256','-verify',public,'-signature',sig],data[:-sl])
    m=validate_manifest(strict_json(data[16:16+ml]));body=data[16+ml:-sl]
    if magic==b'PAP2':
        if len(body)<4:raise ValueError('short asset bundle')
        code_size=struct.unpack_from('<I',body)[0]
        if not 0<code_size<=(MAX_NATIVE_PAYLOAD if m['runtime']=='native-elf' else MAX_PAYLOAD) or 4+code_size>len(body):raise ValueError('code bounds')
        payload=body[4:4+code_size]
        from .assets import validate_index
        validate_index(m.get('assets'),body[4+code_size:])
    else:
        payload=body
        if m.get('assets'):raise ValueError('PAP1 cannot contain assets')
    if m.get('publisher_key_id')!=key_id(public) or m.get('sha256')!=hashlib.sha256(payload).hexdigest():raise ValueError('identity/hash mismatch')
    if len(payload)>(MAX_NATIVE_PAYLOAD if m['runtime']=='native-elf' else MAX_PAYLOAD):raise ValueError('payload too large')
    if m['runtime']!='native-elf':
        payload.decode('utf-8')
        if b'\0' in payload:raise ValueError('embedded NUL')
    validate_entry(m,payload)
    return m,payload

def validate_entry(manifest,payload):
    if manifest['runtime']=='native-elf':
        from .native import validate_elf
        validate_elf(payload,manifest['limits']['native_exec_kib']*1024)
    if manifest['runtime']=='declarative':
        from .declarative import validate_ui
        contract=validate_ui(payload)
        missing=contract['permissions']-set(manifest.get('permissions',[]))
        if missing:raise ValueError('declarative UI requires permissions: '+', '.join(sorted(missing)))
        declared={asset['id'] for asset in manifest.get('assets',[]) if isinstance(asset,dict) and isinstance(asset.get('id'),str)}
        missing_assets=contract['asset_refs']-declared
        if missing_assets:raise ValueError('declarative UI references undeclared assets: '+', '.join(sorted(missing_assets)))
        if contract['fetch_url']:
            from urllib.parse import urlsplit
            url=contract['fetch_url']
            for token in re.findall(r'\{[^{}]*\}',url):url=url.replace(token,'x')
            parsed=urlsplit(url)
            try:port=parsed.port
            except ValueError as error:raise ValueError('declarative UI datasource port') from error
            if parsed.scheme!='https' or not parsed.hostname or (port not in (None,443)) or parsed.hostname not in manifest.get('domains',[]):raise ValueError('declarative UI datasource must use an allowlisted HTTPS domain on port 443')

def unpack_assets(data,public):
    m,code=unpack(data,public)
    if data[:4]!=b'PAP2':return m,code,{}
    ml=struct.unpack_from('<I',data,4)[0];base=16+ml+4+len(code)
    return m,code,{r['id']:data[base+r['offset']:base+r['offset']+r['size']] for r in m['assets']}
