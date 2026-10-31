#!/usr/bin/env python3
"""Build a static distribution from verified firmware and explicitly selected PAPs."""
import argparse
import hashlib
import html
import json
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'sdk'))
from passport import unpack, key_id
from passport.update import verify_firmware_manifest


def regular(directory, name):
    if not isinstance(name, str) or not name or Path(name).name != name or name in ('.', '..'):
        raise ValueError('payload filenames must be simple basenames')
    path = Path(directory) / name
    if path.is_symlink() or not path.is_file():
        raise ValueError('missing or symlinked payload')
    return path


def build(firmware, packages, selection, public, app_keys, output, initial=None, previous=None):
    output = Path(output)
    if output.exists():
        raise ValueError('output must be a new directory; previous site remains intact')
    manifest = verify_firmware_manifest(regular(firmware, 'latest.json').read_bytes(), public)
    image = regular(firmware, manifest['image'])
    verify_firmware_manifest(json.dumps(manifest), public, image.read_bytes())
    entries = json.loads(Path(selection).read_text())['apps']
    if not isinstance(entries, list) or len(entries) > 8:
        raise ValueError('device catalog supports up to eight selected apps')
    files, catalog, ids = [], [], set()
    for entry in entries:
        filename = entry.get('file') or entry.get('url', '').rsplit('/', 1)[-1]
        source = regular(packages, filename); raw = source.read_bytes()
        m = None
        for key in app_keys:
            try:
                m, _ = unpack(raw, key); break
            except ValueError:
                pass
        if m is None:
            raise ValueError('app signature/hash/format invalid')
        if m['id'] != entry['id'] or m['id'] in ids or m['version'] != entry['version']:
            raise ValueError('selection identity/version mismatch or duplicate')
        if m['runtime'] == 'native-elf' and m['publisher_key_id'] != key_id(public):
            raise ValueError('Native app must be signed by this OS trust root')
        ids.add(m['id'])
        name = m['id'] + '-' + m['version'] + '.pap'
        files.append((source, name))
        catalog.append({**{k:m[k] for k in ('id','name','version','publisher_key_id','permissions')},
                        'url':name, 'sha256':hashlib.sha256(raw).hexdigest(), 'size':len(raw),
                        'icon':m.get('icon','system.app-default'),
                        'package_format':2 if raw[:4] == b'PAP2' else 1,
                        'features':(['assets/1'] if m.get('assets') else []) +
                                   (['native-elf/'+str(m['abi'])] if m['runtime']=='native-elf' else [])})
    # Fully validate before creating output. Never copy an input directory wholesale.
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as staging:
        site = Path(staging) / 'site'; (site / 'updates').mkdir(parents=True); (site / 'apps').mkdir()
        shutil.copyfile(image, site / 'updates' / manifest['image'])
        (site / 'updates/latest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        for source, name in files:
            shutil.copyfile(source, site / 'apps' / name)
        (site / 'apps/index.json').write_text(json.dumps({'format':'passport.catalog/1','apps':catalog},indent=2)+'\n')
        (site / 'release-public.pem').write_bytes(Path(public).read_bytes())
        if initial:
            data = Path(initial).read_bytes()
            # Bind community first-install image to the same OTA app and fixed OS layout.
            if len(data) not in (0x20000 + image.stat().st_size, 0x800000) or data[0x20000:0x20000+image.stat().st_size] != image.read_bytes():
                raise ValueError('initial image does not contain the verified OTA application')
            (site / 'downloads').mkdir(); (site / 'downloads/PassportOS-initial-system.bin').write_bytes(data)
        # Retain two previous signed OS images, so deployment does not break
        # clients that already fetched the previous latest.json.
        history=[]
        if previous and (Path(previous)/'updates/latest.json').is_file():
            old=Path(previous)/'updates'
            candidates=[old/'latest.json',*old.glob('previous-*.json')]
            verified=[verify_firmware_manifest(p.read_bytes(),public) for p in candidates]
            seen=set()
            for m in sorted(verified,key=lambda m:m['build'],reverse=True):
                if m['build']>manifest['build']:raise ValueError('cannot replace a newer public release')
                if m['version']==manifest['version']:
                    if m!=manifest:raise ValueError('public version is immutable')
                    continue
                if m['version'] in seen or len(history)>=2:continue
                if m['image']==manifest['image']:raise ValueError('versioned image filename reused')
                raw=regular(old,m['image']).read_bytes()
                verify_firmware_manifest(json.dumps(m),public,raw)
                seen.add(m['version']);name='previous-'+m['version']+'.json'
                (site/'updates'/name).write_text(json.dumps(m)+'\n')
                (site/'updates'/m['image']).write_bytes(raw);history.append(name)
        (site/'updates/history.json').write_text(json.dumps(history)+'\n')
        # Retain one prior package version per still-published ID.
        app_history=[]
        if previous and (Path(previous)/'apps/index.json').is_file():
            old=Path(previous)/'apps';old_entries=json.loads((old/'index.json').read_text())['apps']
            if (old/'history.json').is_file():old_entries+=json.loads((old/'history.json').read_text())
            retained=set();current_files={name for _,name in files}
            by_url={a['url']:a for a in catalog}
            for a in old_entries:
                if a['url'] in current_files:
                    if a['sha256']!=by_url[a['url']]['sha256'] or a['size']!=by_url[a['url']]['size']:raise ValueError('public app version is immutable')
                    continue
                if a['id'] not in ids or a['id'] in retained:continue
                raw=regular(old,a['url']).read_bytes()
                m,_=unpack(raw,public)
                if m['id']!=a['id'] or m['version']!=a['version'] or hashlib.sha256(raw).hexdigest()!=a['sha256'] or len(raw)!=a['size']:
                    raise ValueError('previous package metadata mismatch')
                (site/'apps'/a['url']).write_bytes(raw);retained.add(a['id']);app_history.append(a)
        (site/'apps/history.json').write_text(json.dumps(app_history)+'\n')
        links = ''.join('<li><a href="apps/'+a['url']+'">'+html.escape(a['name'])+'</a> '+html.escape(a['version'])+' · '+str(a['size'])+' B</li>' for a in catalog)
        repository=json.loads((ROOT/'distribution/config.json').read_text())['repository']
        release='https://github.com/'+repository+'/releases/tag/v'+manifest['version']
        (site / 'index.html').write_text('<!doctype html><meta charset="utf-8"><title>Passport OS</title>'
            '<h1>Passport OS</h1><p>Native apps first; Lua remains compatible.</p>'
            '<p>System update / 系统更新: '+html.escape(manifest['version'])+'</p>'
            '<a href="updates/latest.json">Signed update manifest</a> · '
            '<a href="apps/index.json">App catalog</a><ul>'+links+'</ul>'
            '<p><a href="'+html.escape(release)+'">First-install firmware / 首次安装固件</a> · '
            '<a href="https://github.com/'+html.escape(repository)+'">Source / 源码</a></p>'
            '<p>First installation / 首次安装: use the reviewed community firmware and installation guide. '
            'Existing devices / 已安装设备: Settings → System Update.</p>')
        (site / '.nojekyll').touch()
        hashes = {str(p.relative_to(site)):hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in sorted(site.rglob('*')) if p.is_file()}
        (site / 'SHA256SUMS').write_text(''.join(v+'  '+k+'\n' for k,v in hashes.items()))
        shutil.move(str(site), output)
    return {'version':manifest['version'], 'apps':len(catalog), 'files':len(hashes)}


def bundle(site, destination):
    # Include only inputs that can be rebuilt/verified by the Pages job.
    site = Path(site)
    names = ['updates/latest.json']
    manifest = json.loads((site / names[0]).read_text())
    names += ['updates/'+manifest['image'], 'apps/index.json']
    catalog = json.loads((site/'apps/index.json').read_text())
    names += ['apps/'+a['url'] for a in catalog['apps']]
    if (site/'downloads/PassportOS-initial-system.bin').is_file():
        names += ['downloads/PassportOS-initial-system.bin']
    with zipfile.ZipFile(destination, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name in names:
            source = regular(site/Path(name).parent, Path(name).name)
            archive.write(source, name)


def extract(bundle_path, output):
    output=Path(output)
    if output.exists(): raise ValueError('extraction destination must be new')
    with zipfile.ZipFile(bundle_path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or sum(i.file_size for i in archive.infolist()) > 100_000_000:
            raise ValueError('duplicate or oversized release archive')
        for info in archive.infolist():
            name=info.filename; parts=Path(name).parts
            if len(parts)!=2 or parts[0] not in ('apps','updates','downloads') or '..' in parts or '\\' in name or name.startswith('/') or (info.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError('unsafe release archive path')
        output.mkdir(parents=True)
        archive.extractall(output)

def fetch_history(base_url,public,output):
    import urllib.request
    import urllib.error
    from urllib.parse import urlsplit
    base_url=base_url.rstrip('/')+'/'
    if urlsplit(base_url).scheme!='https':raise ValueError('history requires HTTPS')
    output=Path(output)
    if output.exists():raise ValueError('history destination must be new')
    def get(path,optional=False):
        try:
            with urllib.request.urlopen(base_url+path,timeout=90) as response:
                data=response.read(3_000_001)
                if len(data)>3_000_000:raise ValueError('history payload too large')
                return data
        except urllib.error.HTTPError as error:
            if optional and error.code==404:return None
            raise
    latest=get('updates/latest.json',True)
    output.mkdir(parents=True)
    if latest is None:return
    (output/'updates').mkdir();(output/'apps').mkdir()
    names=['latest.json'];history=get('updates/history.json',True)
    if history:
        records=json.loads(history)
        if not isinstance(records,list) or len(records)>2:raise ValueError('invalid history')
        names+=records
    for name in names:
        if Path(name).name!=name or not name.endswith('.json'):raise ValueError('invalid history name')
        raw=latest if name=='latest.json' else get('updates/'+name)
        m=verify_firmware_manifest(raw,public);image=get('updates/'+m['image'])
        verify_firmware_manifest(raw,public,image)
        (output/'updates'/name).write_bytes(raw);(output/'updates'/m['image']).write_bytes(image)
    catalog=get('apps/index.json');entries=json.loads(catalog)['apps'];older=get('apps/history.json',True)
    previous=json.loads(older) if older else []
    if len(entries)>8 or len(previous)>8:raise ValueError('invalid app history')
    for a in entries+previous:
        name=a['url']
        if Path(name).name!=name:raise ValueError('invalid historical package name')
        raw=get('apps/'+name);m,_=unpack(raw,public)
        if m['id']!=a['id'] or m['version']!=a['version'] or len(raw)!=a['size'] or hashlib.sha256(raw).hexdigest()!=a['sha256']:raise ValueError('invalid app history')
        (output/'apps'/name).write_bytes(raw)
    (output/'apps/index.json').write_bytes(catalog)
    (output/'apps/history.json').write_text(json.dumps(previous))

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__); sub=p.add_subparsers(dest='command',required=True)
    b=sub.add_parser('build');b.add_argument('--firmware',required=True);b.add_argument('--packages',required=True)
    b.add_argument('--selection',required=True);b.add_argument('--public',required=True)
    b.add_argument('--app-key',action='append',default=[]);b.add_argument('--output',required=True);b.add_argument('--initial');b.add_argument('--previous')
    z=sub.add_parser('bundle');z.add_argument('site');z.add_argument('output')
    x=sub.add_parser('extract');x.add_argument('bundle');x.add_argument('output')
    h=sub.add_parser('history');h.add_argument('--base-url',required=True);h.add_argument('--public',required=True);h.add_argument('--output',required=True)
    a=p.parse_args()
    if a.command=='build': print(json.dumps(build(a.firmware,a.packages,a.selection,a.public,a.app_key or [a.public],a.output,a.initial,a.previous)))
    elif a.command=='bundle': bundle(a.site,a.output)
    elif a.command=='extract': extract(a.bundle,a.output)
    else: fetch_history(a.base_url,a.public,a.output)
