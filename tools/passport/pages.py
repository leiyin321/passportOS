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


def build(firmware, packages, selection, public, app_keys, output, initial=None):
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

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__); sub=p.add_subparsers(dest='command',required=True)
    b=sub.add_parser('build');b.add_argument('--firmware',required=True);b.add_argument('--packages',required=True)
    b.add_argument('--selection',required=True);b.add_argument('--public',required=True)
    b.add_argument('--app-key',action='append',default=[]);b.add_argument('--output',required=True);b.add_argument('--initial')
    z=sub.add_parser('bundle');z.add_argument('site');z.add_argument('output')
    x=sub.add_parser('extract');x.add_argument('bundle');x.add_argument('output')
    a=p.parse_args()
    if a.command=='build': print(json.dumps(build(a.firmware,a.packages,a.selection,a.public,a.app_key or [a.public],a.output,a.initial)))
    elif a.command=='bundle': bundle(a.site,a.output)
    else: extract(a.bundle,a.output)
