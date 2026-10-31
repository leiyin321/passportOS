#!/usr/bin/env python3
"""Resumable sector-aligned Flash backup, private artifacts only."""
import subprocess,sys,hashlib,json
from pathlib import Path
root=Path('local/flash-backup');root.mkdir(exist_ok=True)
for i in range(16):
 p=root/f'{i*0x80000:06x}.bin'
 if p.exists() and p.stat().st_size==0x80000:continue
 with (root/f'{i:02d}.log').open('w') as log:
  subprocess.run([sys.executable,'-m','esptool','--chip','esp32c3','-p','/dev/cu.usbmodem101','-b','460800','read_flash',hex(i*0x80000),'0x80000',str(p)],stdout=log,stderr=subprocess.STDOUT,check=True)
 print(f'backup chunk {i+1}/16',flush=True)
full=b''.join((root/f'{i*0x80000:06x}.bin').read_bytes() for i in range(16))
Path('local/original-flash.bin').write_bytes(full)
Path('local/original-flash.sha256').write_text(hashlib.sha256(full).hexdigest()+'\n')
print('8MB backup complete',flush=True)
