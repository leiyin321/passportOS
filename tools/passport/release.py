#!/usr/bin/env python3
"""Assemble a local release from an independently verified firmware archive."""
import argparse
import hashlib
import json
import os
import tempfile
import shutil
import subprocess
import sys
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('archive', type=Path)
p.add_argument('--output', type=Path, default=Path('build/release'))
a = p.parse_args()
subprocess.run([sys.executable, 'tools/archive_firmware.py', 'verify', str(a.archive)], check=True)
a.output.mkdir(parents=True, exist_ok=True)
files = {'FoloToy-AI-Passport.bin': 'PassportOS-app.bin',
         'FoloToy-AI-Passport-full.bin': 'PassportOS-initial-system.bin',
         'bootloader/bootloader.bin': 'bootloader.bin',
         'partition_table/partition-table.bin': 'partition-table.bin',
         'manifest.json': 'build-manifest.json'}
for source, target in files.items():
    shutil.copyfile(a.archive / source, a.output / target)
merged = (a.output / 'PassportOS-initial-system.bin').read_bytes()
(a.output / 'ota-data-initial.bin').write_bytes(merged[0x10000:0x12000])
apps = a.output / 'dist'
apps.mkdir(exist_ok=True)
for source in Path('examples/packages').glob('*.pap'):
    shutil.copyfile(source, apps / source.name)
shutil.copyfile('sdk/publisher-public.pem', a.output / 'publisher-public.pem')
shutil.copyfile('LICENSE', a.output / 'LICENSE')
if Path('build/reports/assets-size-components-final.txt').exists():
    shutil.copyfile('build/reports/assets-size-components-final.txt', a.output / 'size-components.txt')
for source in [Path('sdk'), Path('protocol'), Path('docs'), Path('examples'), Path('simulator'), Path('tools/passport_assets'), Path('os'), Path('components/passport_lua')]:
    shutil.copytree(source, a.output / source, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '*.egg-info'))
idf = Path(os.environ['IDF_PATH'])
with tempfile.TemporaryDirectory() as empty:
    subprocess.run([sys.executable, str(idf / 'components/spiffs/spiffsgen.py'), '0x260000', empty, str(a.output / 'packages-EMPTY.bin'), '--obj-name-len', '96', '--meta-len', '4'], check=True)
(a.output / 'appdata-EMPTY.bin').write_bytes(b'\xff' * 0x80000)
manifest = {'format': 'passport.release/1', 'chip': 'esp32c3', 'flash_bytes': 8388608,
            'application_bytes': (a.output / 'PassportOS-app.bin').stat().st_size,
            'application_offset': '0x20000',
            'application_sha256': hashlib.sha256((a.output / 'PassportOS-app.bin').read_bytes()).hexdigest(),
            'warning': 'Initial system image replaces boot/NVS/partition/OTA metadata. Existing board: app-only update. Never initialize storage over existing data.',
            'source_archive': a.archive.name}
(a.output / 'release.json').write_text(json.dumps(manifest, indent=2) + '\n')
paths = sorted(x for x in a.output.rglob('*') if x.is_file() and x.name != 'SHA256SUMS')
(a.output / 'SHA256SUMS').write_text(''.join(hashlib.sha256(x.read_bytes()).hexdigest() + '  ' + str(x.relative_to(a.output)) + '\n' for x in paths))
print(a.output.resolve())
