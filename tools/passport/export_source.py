#!/usr/bin/env python3
"""Export the current source snapshot without private history or build diagnostics."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[2]

def export(destination):
    destination=Path(destination).resolve()
    if destination.exists():raise ValueError('source destination must be new')
    raw=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=ROOT)
    names=sorted(set(n.decode() for n in raw.split(b'\0') if n))
    keep=[];excluded=[]
    for name in names:
        p=ROOT/name
        if not p.is_file() or p.is_symlink():continue
        diagnostic='/logs/' in '/'+name or p.suffix=='.log' or p.name.endswith('results.txt')
        generated=(p.suffix in ('.elf','.bin','.map') and not name.startswith('tests/')) or '.egg-info/' in name
        if diagnostic or generated:excluded.append(name)
        else:keep.append(name)
    # Secret scan before any output, without echoing the matching contents.
    forbidden=re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|(?:gh[pousr]_|github_pat_)[A-Za-z0-9_]{20,}')
    for name in keep:
        if forbidden.search((ROOT/name).read_bytes()):raise ValueError('sensitive material in '+name)
    destination.mkdir(parents=True)
    excluded_set=set(excluded)
    for name in keep:
        source=ROOT/name;target=destination/name;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        if source.suffix=='.md':
            text=target.read_text()
            def link(match):
                label,ref=match.groups();ref=ref.split('#')[0]
                resolved=(source.parent/ref).resolve()
                try:rel=str(resolved.relative_to(ROOT))
                except ValueError:return match.group(0)
                if rel not in excluded_set:return match.group(0)
                return label+' (`'+ref+'`; '+('本地诊断，不公开' if name.endswith('.zh_CN.md') else 'local diagnostics, not published')+')'
            text=re.sub(r'\[([^\]]+)\]\(([^)]+)\)',link,text)
            text=re.sub(r'/Users/[^/\s`]+/', '/path/to/user/',text)
            # Device identifiers in historical reports are not source contracts.
            text=re.sub(r'(?i)\b(?:[0-9a-f]{2}:){5}[0-9a-f]{2}\b','XX:XX:XX:XX:XX:XX',text)
            target.write_text(text)
    subprocess.run(['git','init','-b','main',str(destination)],check=True,capture_output=True)
    repository=json.loads((ROOT/'distribution/config.json').read_text())['repository']
    subprocess.run(['git','remote','add','origin','https://github.com/'+repository+'.git'],cwd=destination,check=True)
    print(json.dumps({'files':len(keep),'excluded_diagnostics_or_generated':len(excluded),'destination':str(destination)}))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('destination');a=p.parse_args();export(a.destination)
