#!/usr/bin/env python3
"""Compile unsigned Native examples and check curated source identities in CI."""
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'sdk'))
from passport import validate_manifest
from passport.native import build

selected=json.loads((ROOT/'distribution/apps.json').read_text())['apps']
for entry in selected:
    source=(ROOT/entry['source']).resolve()
    if not source.is_relative_to(ROOT):raise ValueError('source must stay within repository')
    m=json.loads((source/'manifest.json').read_text())
    if (m['id'],m['version'])!=(entry['id'],entry['version']):raise ValueError('curated source identity/version mismatch')
count=0
for path in sorted((ROOT/'examples').glob('*/manifest.json')):
    m=json.loads(path.read_text())
    if m.get('runtime')!='native-elf':continue
    validate_manifest(m)
    output,info=build(path.parent)
    print(json.dumps({'id':m['id'],'version':m['version'],'elf_bytes':output.stat().st_size,'layout':info}))
    count+=1
if not count:raise ValueError('no Native examples compiled')
print('Unsigned Native source builds: PASS; count='+str(count))
