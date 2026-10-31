import unittest,sys,struct,tempfile,json,subprocess
from pathlib import Path
sys.path.insert(0,'sdk')
from passport.assets import compile_asset,validate_blob,decode_blob,build_manifest_assets,validate_index
from passport import unpack_assets
class Compiler(unittest.TestCase):
 def test_i4_size(self):
  data=compile_asset('examples/weather/assets/icon.png',width=48,height=48,encoding='I4');info=validate_blob(data);self.assertEqual(len(data),1240);self.assertEqual(info['stride']*info['height'],1152);im=decode_blob(data);self.assertEqual(im.getpixel((0,0))[3],0);self.assertEqual(im.getpixel((24,28))[3],255)
 def test_a1_size(self):
  data=compile_asset('examples/icon_gallery/assets/mask.svg',width=24,height=24,encoding='A1');self.assertEqual(len(data),96);self.assertEqual(validate_blob(data)['stride']*24,72)
 def test_repeatable(self):
  args=dict(width=48,height=48,encoding='I4');self.assertEqual(compile_asset('examples/weather/assets/icon.png',**args),compile_asset('examples/weather/assets/icon.png',**args))
 def test_palette_truncation(self):
  data=Path('tests/assets/fixtures/i4.pai').read_bytes()
  with self.assertRaises(ValueError):validate_blob(data[:60])
 def test_palette_index(self):
  data=bytearray(Path('tests/assets/fixtures/i4.pai').read_bytes());data[88]=255
  with self.assertRaises(ValueError):validate_blob(data)
 def test_dimensions_flags(self):
  data=Path('tests/assets/fixtures/i4.pai').read_bytes()
  for offset,value in [(7,1),(5,8),(6,9),(8,255),(12,255),(20,1)]:
   bad=bytearray(data);bad[offset]=value
   with self.assertRaises(ValueError):validate_blob(bad)
 def test_source_bounds(self):
  with self.assertRaises(ValueError):compile_asset('examples/weather/assets/icon.png',width=100000,height=100000)
 def test_source_traversal(self):
  with self.assertRaises(ValueError):build_manifest_assets('examples/weather',[{'id':'icons/test','source':'../../sdk/publisher-public.pem'}])
 def test_duplicate_id(self):
  entry={'id':'icons/test','source':'assets/icon.png'}
  with self.assertRaises(ValueError):build_manifest_assets('examples/weather',[entry,entry])
 def test_external_svg(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d,'bad.svg');p.write_text('<svg xmlns="http://www.w3.org/2000/svg"><image href="file:///private/data"/></svg>')
   with self.assertRaises(ValueError):compile_asset(p,width=24,height=24)
 def test_corrupt_hash(self):
  records,blob=build_manifest_assets('examples/weather',[{'id':'icons/test','source':'assets/icon.png','width':48,'height':48,'encoding':'I4'}]);records[0]['sha256']='0'*64
  with self.assertRaises(ValueError):validate_index(records,blob)
 def test_grid_package(self):
  m,code,assets=unpack_assets(Path('examples/packages/appstore_grid.pap').read_bytes(),'sdk/publisher-public.pem');self.assertEqual(len(assets),30);self.assertEqual(sum(map(len,assets.values())),37200);self.assertLess(len(Path('examples/packages/appstore_grid.pap').read_bytes()),100000)
if __name__=='__main__':unittest.main(verbosity=2)
