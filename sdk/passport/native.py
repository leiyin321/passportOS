"""Freestanding ESP32-C3 native build and strict ELF v1 preflight."""
import shutil
import struct
import subprocess
from pathlib import Path

def sdk_root():
    installed=Path(__file__).resolve().parent/'_native'
    return installed if installed.is_dir() else Path(__file__).resolve().parents[1]/'native'

def validate_elf(data, capacity=16384):
    def fail(): raise ValueError('unsupported or malformed native ELF')
    def bounds(off, length):
        if not 0 <= off <= len(data) or not 0 <= length <= len(data)-off: fail()
    if len(data)<52: fail()
    h=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    if h[0][:7]!=b'\x7fELF\x01\x01\x01' or h[1:4]!=(3,243,1) or h[7]&~1 or h[8]!=52 or h[9]!=32 or h[11]!=40: fail()
    entry,pho,sho,phn,shn,names=h[4],h[5],h[6],h[10],h[12],h[13]
    if not 0<phn<=8 or not 0<shn<=64 or names>=shn: fail()
    bounds(pho,phn*32);bounds(sho,shn*40)
    loads=[];end=0
    for i in range(phn):
        p=struct.unpack_from('<8I',data,pho+i*32)
        if p[0]==7: fail()
        if p[0]!=1: continue
        bounds(p[1],p[4])
        if p[4]>p[5] or p[2]+p[5]>capacity or (not loads and p[2]) or p[2]<end or p[7] and p[7]&(p[7]-1): fail()
        loads.append(p);end=p[2]+p[5]
    def loaded(address,length,flags):
        return any(p[6]&flags==flags and p[2]<=address and address+length<=p[2]+(p[4] if flags&1 else p[5]) for p in loads)
    if not loads or not end or entry&1 or not loaded(entry,2,1): fail()
    sections=[struct.unpack_from('<10I',data,sho+i*40) for i in range(shn)]
    ns=sections[names]
    if ns[1]!=3: fail()
    bounds(ns[4],ns[5])
    def string(section,offset):
        bounds(section[4],section[5])
        if offset>=section[5] or data.find(b'\0',section[4]+offset,section[4]+section[5])<0: fail()
    for s in sections:
        if s[2]&0x400 or s[1] in (9,14,15,16) or s[6]>=shn or s[8] and s[8]&(s[8]-1): fail()
        if s[1]!=8: bounds(s[4],s[5])
        string(ns,s[0])
        if s[1] in (2,11):
            strings=sections[s[6]]
            if s[9]!=16 or s[5]%16 or strings[1]!=3: fail()
            for j in range(0,s[5],16):
                symbol=struct.unpack_from('<IIIBBH',data,s[4]+j)
                string(strings,symbol[0])
                if symbol[5]==0 and symbol[0]: fail()
        if s[1]==4:
            symbols=sections[s[6]]
            if s[9]!=12 or s[5]%12 or symbols[1] not in (2,11) or symbols[5]<16: fail()
            for j in range(0,s[5],12):
                where,info,addend=struct.unpack_from('<III',data,s[4]+j)
                if info!=3 or where&3 or not loaded(where,4,2) or not loaded(addend,1,0): fail()
    return {'target':'esp32c3','abi':'manifest-defined','entry':entry,'exec_bytes':end,'file_bytes':len(data)}

def build(directory):
    directory=Path(directory).resolve()
    compiler=shutil.which('riscv32-esp-elf-gcc')
    if not compiler: raise ValueError('activate ESP-IDF 5.5.3 export.sh before native build')
    sources=sorted(directory.glob('*.c'))
    if not sources: raise ValueError('no C sources')
    output=directory/'app.app.elf'
    include=sdk_root()/'include'
    cmd=[compiler,'-march=rv32imc_zicsr_zifencei','-mabi=ilp32','-Os','-fPIC','-fvisibility=hidden','-fno-builtin','-fno-tree-loop-distribute-patterns','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-nostdlib','-nostartfiles','-shared','-Wl,-e,passport_app_entry','-Wl,--no-relax','-Wl,-z,max-page-size=512','-I'+str(include),*[str(p) for p in sources],str(include.parent/'support.c'),'-o',str(output)]
    try: subprocess.run(cmd,check=True)
    except subprocess.CalledProcessError as error: raise ValueError("native compilation failed; see compiler output") from error
    return output,validate_elf(output.read_bytes())
