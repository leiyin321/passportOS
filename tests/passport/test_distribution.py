import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile
sys.path.insert(0,'sdk')
from passport import pack
from passport.update import publish_firmware

def module(name):
    spec=importlib.util.spec_from_file_location(name,'tools/passport/'+name+'.py')
    mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);return mod
pages=module('pages');config=module('configure_distribution')

class Distribution(unittest.TestCase):
    def test_build_verify_relative_paths_and_fork_key(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);key=root/'private.pem';public=root/'public.pem'
            subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',key],check=True,capture_output=True)
            subprocess.run(['openssl','pkey','-in',key,'-pubout','-out',public],check=True,capture_output=True)
            config.generate(root/'headers','https://example.github.io/fork',public)
            self.assertIn('/fork/apps/index.json',(root/'headers/distribution_defaults.h').read_text())
            with self.assertRaises(ValueError):config.generate(root/'bad','http://example.com',public)
            with self.assertRaises(ValueError):config.generate(root/'bad','https://example.com',key)
            image=root/'app.bin';image.write_bytes(b'\xe9'+b'X'*8191)
            publish_firmware(image,'2.0.0',1,key,public,root/'updates')
            app=root/'app';app.mkdir()
            manifest=dict(format='passport.app/1',id='org.test.demo',name='Demo',version='1.0.0',runtime='lua',entry='main.lua',min_os_api=1,data_schema=1,permissions=['ui'],limits=dict(script_heap_kib=16,fps=10,persistent_kib=4))
            (app/'manifest.json').write_text(json.dumps(manifest));(app/'main.lua').write_text('function on_start() end')
            pap=root/'demo.pap';pap.write_bytes(pack(app,key,public))
            selection=root/'selection.json';selection.write_text(json.dumps({'apps':[dict(id='org.test.demo',version='1.0.0',file='demo.pap')]}))
            site=root/'site';pages.build(root/'updates',root,selection,public,[public],site)
            catalog=json.loads((site/'apps/index.json').read_text());self.assertEqual(catalog['apps'][0]['url'],'org.test.demo-1.0.0.pap')
            archive=root/'release.zip';pages.bundle(site,archive);pages.extract(archive,root/'inputs')
            pages.build(root/'inputs/updates',root/'inputs/apps',root/'inputs/apps/index.json',public,[public],root/'rebuilt')
            self.assertEqual((site/'apps/index.json').read_bytes(),(root/'rebuilt/apps/index.json').read_bytes())
            with self.assertRaises(ValueError):pages.build(root/'updates',root,selection,public,[public],site)
            pap.write_bytes(pap.read_bytes()[:-1]+b'!')
            with self.assertRaises(ValueError):pages.build(root/'updates',root,selection,public,[public],root/'corrupt')
            self.assertFalse((root/'corrupt').exists())
            bad=root/'bad.zip'
            with zipfile.ZipFile(bad,'w') as z:z.writestr('../escape','bad')
            with self.assertRaises(ValueError):pages.extract(bad,root/'escape')

if __name__=='__main__':unittest.main()
