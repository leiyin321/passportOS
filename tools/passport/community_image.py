#!/usr/bin/env python3
"""Create a first-install 8 MiB image with clean Passport package/data areas."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('archive',type=Path);p.add_argument('output',type=Path)
a=p.parse_args()
subprocess.run([sys.executable,'tools/archive_firmware.py','verify',str(a.archive)],check=True)
source=(a.archive/'FoloToy-AI-Passport-full.bin').read_bytes()
# This delivery format is deliberately for the current Passport OS layout only.
if len(source)>0x2a0000:raise ValueError('unexpected initial image layout')
image=bytearray(b'\xff'*0x800000);image[:len(source)]=source
with tempfile.TemporaryDirectory() as tmp:
    empty=Path(tmp)/'empty';empty.mkdir();filesystem=Path(tmp)/'packages.bin'
    subprocess.run([sys.executable,str(Path(os.environ['IDF_PATH'])/'components/spiffs/spiffsgen.py'),
                    '0x260000',str(empty),str(filesystem),'--obj-name-len','96','--meta-len','4'],check=True)
    image[0x520000:0x780000]=filesystem.read_bytes()
a.output.parent.mkdir(parents=True,exist_ok=True)
with a.output.open('xb') as out:out.write(image)
print('First-install image: 8388608 bytes; replaces existing settings/apps/data')
