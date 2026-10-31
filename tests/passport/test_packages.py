import unittest,sys,struct,json,subprocess,tempfile,random
from pathlib import Path
sys.path.insert(0,'sdk')
from passport import pack,unpack,strict_json,validate_manifest
PUB='sdk/publisher-public.pem'
class Packages(unittest.TestCase):
 def test_examples(self):
  for f in Path('examples/packages').glob('*.pap'):
   m,p=unpack(f.read_bytes(),PUB);self.assertTrue(m['id'].startswith('org.passport.'));self.assertTrue(p)
 def test_repeatable(self):
  with tempfile.TemporaryDirectory() as d:
   key=Path(d)/'private.pem';public=Path(d)/'public.pem'
   subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',str(key)],check=True,capture_output=True)
   subprocess.run(['openssl','pkey','-in',str(key),'-pubout','-out',str(public)],check=True,capture_output=True)
   a=pack('examples/counter',key,public);b=pack('examples/counter',key,public);self.assertEqual(a,b)
 def test_tamper(self):
  b=bytearray(Path('examples/packages/counter.pap').read_bytes());b[100]^=1
  with self.assertRaises(ValueError):unpack(b,PUB)
 def test_duplicate(self):
  with self.assertRaises(ValueError):strict_json('{"id":"a","id":"b"}')
 def test_display_manifest(self):
  m=json.loads(Path('examples/counter/manifest.json').read_text())
  m['display']={'mode':'fullscreen','orientation':'portrait','navigation':'game','design_width':240,'design_height':320}
  self.assertEqual(validate_manifest(m)['display']['mode'],'fullscreen')
  for bad in [{'mode':'rotated'}, {'mode':'fullscreen','orientation':'landscape'}, {'mode':'standard','extra':True}, {'mode':'standard','navigation':'freeform'}]:
   m['display']=bad
   with self.assertRaises(ValueError):validate_manifest(m)
 def test_privilege(self):
  m=json.loads(Path('examples/counter/manifest.json').read_text());m['permissions'].append('system.ota')
  with self.assertRaises(ValueError):validate_manifest(m)
 def test_extended_lua_heap_bound(self):
  m=json.loads(Path('examples/counter/manifest.json').read_text())
  m['limits']['script_heap_kib']=64
  self.assertEqual(validate_manifest(m)['limits']['script_heap_kib'],64)
  m['limits']['script_heap_kib']=65
  with self.assertRaises(ValueError):validate_manifest(m)
 def test_path(self):
  m=json.loads(Path('examples/counter/manifest.json').read_text());m['entry']='../../secret'
  with self.assertRaises(ValueError):validate_manifest(m)
 def test_schema_accepts_authoring_and_packaged_manifest_shapes(self):
  schema=json.loads(Path('protocol/manifest-v1.schema.json').read_text())
  self.assertEqual(schema['$schema'],'https://json-schema.org/draft/2020-12/schema')
  self.assertIn('sourceAsset',schema['$defs']);self.assertIn('compiledAsset',schema['$defs'])
  ui_schema=json.loads(Path('protocol/ui-v1.schema.json').read_text())
  catalog_schema=json.loads(Path('protocol/catalog-v1.schema.json').read_text())
  self.assertEqual(ui_schema['$id'],'urn:passport:ui:1')
  self.assertEqual(catalog_schema['properties']['apps']['maxItems'],8)
  for directory in Path('examples').iterdir():
   manifest=directory/'manifest.json'
   if manifest.exists():validate_manifest(json.loads(manifest.read_text()))
  for path in Path('examples/packages').glob('*.pap'):
   manifest,_=unpack(path.read_bytes(),PUB)
   validate_manifest(manifest)
 def test_publish_index_accepts_multiple_publishers(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td);dist=root/'dist';dist.mkdir();keys=[]
   for i in range(2):
    private=root/f'key{i}.pem';public=root/f'key{i}-pub.pem'
    subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',str(private)],check=True,capture_output=True)
    subprocess.run(['openssl','pkey','-in',str(private),'-pubout','-out',str(public)],check=True,capture_output=True)
    app=root/f'app{i}';app.mkdir();m=json.loads(Path('examples/counter/manifest.json').read_text());m['id']=f'org.test.app{i}';m['name']=f'Test {i}';(app/'manifest.json').write_text(json.dumps(m));(app/'ui.json').write_bytes(Path('examples/counter/ui.json').read_bytes())
    (dist/f'app{i}.pap').write_bytes(pack(app,private,public));keys.append(public)
   output=root/'index.json'
   subprocess.run([sys.executable,'-m','passport','publish-index',str(dist),'--public',str(keys[0]),'--public',str(keys[1]),'-o',str(output)],check=True,env={**__import__('os').environ,'PYTHONPATH':'sdk'},capture_output=True)
   catalog=json.loads(output.read_text());self.assertEqual(catalog['format'],'passport.catalog/1');self.assertEqual([a['id'] for a in catalog['apps']],['org.test.app0','org.test.app1'])
 def test_publish_index_enforces_device_entry_limit(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td);dist=root/'dist';dist.mkdir();key=root/'private.pem';public=root/'public.pem'
   subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',str(key)],check=True,capture_output=True)
   subprocess.run(['openssl','pkey','-in',str(key),'-pubout','-out',str(public)],check=True,capture_output=True)
   for i in range(9):
    app=root/f'app{i}';app.mkdir();m=json.loads(Path('examples/counter/manifest.json').read_text());m['id']=f'org.limit.test{i}';m['name']=f'Limit {i}';(app/'manifest.json').write_text(json.dumps(m));(app/'ui.json').write_bytes(Path('examples/counter/ui.json').read_bytes())
    (dist/f'{i:02}.pap').write_bytes(pack(app,key,public))
   result=subprocess.run([sys.executable,'-m','passport','publish-index',str(dist),'--public',str(public),'-o',str(root/'index.json')],env={**__import__('os').environ,'PYTHONPATH':'sdk'},capture_output=True)
   self.assertNotEqual(result.returncode,0);self.assertIn(b'at most 8 apps',result.stderr)
 def test_bounds_fuzz(self):
  rng=random.Random(57);original=Path('examples/packages/counter.pap').read_bytes()
  for _ in range(1000):
   b=bytearray(original);field=rng.choice([4,8,12]);struct.pack_into('<I',b,field,rng.getrandbits(32))
   with self.assertRaises(ValueError):unpack(b,PUB)
 def test_truncation(self):
  b=Path('examples/packages/counter.pap').read_bytes()
  for n in [0,1,15,16,100,len(b)-1,len(b)+1]:
   with self.assertRaises(ValueError):unpack((b+b'0')[:n],PUB)
if __name__=='__main__':unittest.main(verbosity=2)
