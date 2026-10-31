import ctypes
import json
import os
import random
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,'sdk')
from passport import pack,unpack,validate_manifest
from passport.native import validate_elf

def fixture():
    data=bytearray(0x340)
    data[:52]=struct.pack('<16sHHIIIIIHHHHHH',b'\x7fELF\x01\x01\x01'+bytes(9),3,243,1,0x80,52,0x200,1,52,32,2,40,8,7)
    struct.pack_into('<8I',data,52,1,0,0,0,0x100,0x100,5,0x100)
    struct.pack_into('<8I',data,84,1,0x100,0x300,0x300,16,32,6,0x100)
    data[0x80:0x88]=bytes.fromhex('1305a00267800000')
    names=b'\0.text\0.data\0.bss\0.dynstr\0.dynsym\0.rela.dyn\0.shstrtab\0'
    data[0x160:0x160+len(names)]=names
    sections=[(0,0,0,0,0,0,0,0,0,0),
      (1,1,6,0x80,0x80,8,0,0,4,0),(7,1,3,0x300,0x100,16,0,0,4,0),
      (13,8,3,0x310,0x110,16,0,0,4,0),(18,3,0,0,0x120,1,0,0,1,0),
      (26,11,0,0,0x130,16,4,0,4,16),(34,4,0,0,0x140,12,5,0,4,12),
      (44,3,0,0,0x160,len(names),0,0,1,0)]
    # derive section-name offsets instead of hard-coding their spelling lengths
    for i,name in enumerate([b'',b'.text',b'.data',b'.bss',b'.dynstr',b'.dynsym',b'.rela.dyn',b'.shstrtab']):
        section=list(sections[i]);section[0]=names.find(name+b'\0')
        struct.pack_into('<10I',data,0x200+i*40,*section)
    struct.pack_into('<III',data,0x140,0x300,3,0x80)
    return bytes(data)

class Native(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
    cls.tmp=tempfile.TemporaryDirectory();cls.root=Path(cls.tmp.name)
    lib=cls.root/'validator.dylib'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC','-Ios/runtime','os/runtime/native_elf.c','-o',str(lib)],check=True)
    cls.lib=ctypes.CDLL(str(lib));cls.validate=cls.lib.ps_native_elf_validate
    cls.validate.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_size_t,ctypes.POINTER(ctypes.c_uint32)]
    cls.validate.restype=ctypes.c_bool
    cls.private=cls.root/'private.pem';cls.public=cls.root/'public.pem'
    subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',str(cls.private)],check=True,capture_output=True)
    subprocess.run(['openssl','pkey','-in',str(cls.private),'-pubout','-out',str(cls.public)],check=True,capture_output=True)
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def accepts(self,data,capacity=16384):
    return self.validate(data,len(data),capacity,None)
 def test_valid_binary_and_signed_package(self):
    data=fixture();self.assertTrue(self.accepts(data));self.assertEqual(validate_elf(data)['exec_bytes'],0x320)
    app=self.root/'app';app.mkdir(exist_ok=True)
    m=json.loads(Path('examples/native_hello/manifest.json').read_text())
    (app/'manifest.json').write_text(json.dumps(m));(app/'app.app.elf').write_bytes(data)
    package=pack(app,self.private,self.public);manifest,payload=unpack(package,self.public)
    self.assertEqual(payload,data);self.assertEqual(manifest['runtime'],'native-elf')
 def test_target_abi_trust_and_permissions(self):
    original=json.loads(Path('examples/native_hello/manifest.json').read_text())
    for key,value in [('abi',True),('abi',3),('trusted',False),('target','esp32c6'),('entry','../../x')]:
      m={**original,key:value}
      with self.assertRaises(ValueError):validate_manifest(m)
    m={**original,'permissions':['network.websocket']}
    with self.assertRaises(ValueError):validate_manifest(m)
 def test_abi2_extended_permissions(self):
    m=json.loads(Path('examples/xiaozhi_native/manifest.json').read_text());validate_manifest(m)
    m['abi']=1
    with self.assertRaises(ValueError):validate_manifest(m)
 def test_hid_permission_requires_native_abi2(self):
    m=json.loads(Path('examples/bluetooth_peripheral/manifest.json').read_text());validate_manifest(m)
    m['abi']=1
    with self.assertRaises(ValueError):validate_manifest(m)
    m=json.loads(Path('examples/counter/manifest.json').read_text());m['permissions'].append('bluetooth.hid')
    with self.assertRaises(ValueError):validate_manifest(m)
 def test_media_permission_requires_native_abi2(self):
    m=json.loads(Path('examples/city_radio/manifest.json').read_text());validate_manifest(m)
    m['abi']=1
    with self.assertRaises(ValueError):validate_manifest(m)
    m=json.loads(Path('examples/counter/manifest.json').read_text());m['permissions'].append('audio.playback')
    with self.assertRaises(ValueError):validate_manifest(m)
 def test_public_audio_http_permission_is_explicit_and_scoped(self):
    m=json.loads(Path('examples/city_radio/manifest.json').read_text());validate_manifest(m)
    m['permissions'].remove('audio.playback')
    with self.assertRaises(ValueError):validate_manifest(m)
    m=json.loads(Path('examples/counter/manifest.json').read_text());m['permissions'].append('audio.http')
    with self.assertRaises(ValueError):validate_manifest(m)
 def test_runtime_specific_fields(self):
    m=json.loads(Path('examples/counter/manifest.json').read_text());m['abi']=1
    with self.assertRaises(ValueError):validate_manifest(m)
    m=json.loads(Path('examples/native_hello/manifest.json').read_text());m['limits']['script_heap_kib']=1
    with self.assertRaises(ValueError):validate_manifest(m)
 def test_relocation_entry_imports_capacity(self):
    for off,value in [(0x144,2),(24,0x310),(36,4),(0x204+40*6,9),(0x200+40*5+24,99)]:
      data=bytearray(fixture());struct.pack_into('<I',data,off,value)
      self.assertFalse(self.accepts(bytes(data)),(off,value))
      with self.assertRaises(ValueError):validate_elf(data)
    self.assertFalse(self.accepts(fixture(),512))
    with self.assertRaises(ValueError):validate_elf(fixture(),512)
 def test_fuzz_parity(self):
    rng=random.Random(975)
    for _ in range(2500):
      data=bytearray(fixture())
      for __ in range(rng.randrange(1,5)):
        i=rng.randrange(len(data));data[i]^=rng.randrange(1,256)
      data=bytes(data[:rng.randrange(len(data)+1)]) if rng.randrange(3)==0 else bytes(data)
      try: validate_elf(data);python_ok=True
      except ValueError:python_ok=False
      self.assertEqual(self.accepts(data),python_ok)
 def test_sanitized_parser(self):
    source=self.root/'fixture.elf';source.write_bytes(fixture());exe=self.root/'parser'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-Ios/runtime','tests/passport/test_native_elf.c','os/runtime/native_elf.c','-o',str(exe)],check=True)
    subprocess.run([str(exe),str(source)],check=True,capture_output=True)
 def test_sdk_init_native(self):
    app=self.root/'new-app'
    subprocess.run([sys.executable,'-m','passport','init',str(app),'--id','org.test.native','--runtime','native-elf'],check=True,env={**os.environ,'PYTHONPATH':'sdk'},capture_output=True)
    self.assertTrue((app/'main.c').is_file());validate_manifest(json.loads((app/'manifest.json').read_text()))

if __name__=='__main__':unittest.main(verbosity=2)
