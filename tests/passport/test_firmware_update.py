import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

sys.path.insert(0, 'sdk')
from passport.update import publish_firmware, verify_firmware_manifest


class FirmwareUpdate(unittest.TestCase):
    def test_signed_release_and_tamper_rejection(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            key, public = root / 'private.pem', root / 'public.pem'
            subprocess.run(['openssl', 'genpkey', '-algorithm', 'RSA',
                            '-pkeyopt', 'rsa_keygen_bits:2048', '-out', key],
                           check=True, capture_output=True)
            subprocess.run(['openssl', 'pkey', '-in', key, '-pubout',
                            '-out', public], check=True, capture_output=True)
            image = root / 'app.bin'
            image_data = b'passport-firmware-image' * 300
            image.write_bytes(image_data)
            output = root / 'release'
            manifest = publish_firmware(image, '1.2.3', 123, key, public, output)
            raw = (output / 'latest.json').read_bytes()
            self.assertEqual(verify_firmware_manifest(raw, public, image_data),
                             manifest)
            with self.assertRaises(ValueError):
                verify_firmware_manifest(raw, public, image_data + b'x')
            from passport.files import atomic_write
            def interrupted(path, data):
                if Path(path).name == 'latest.json': raise OSError('interrupted publication')
                atomic_write(path, data)
            with patch('passport.update.atomic_write', side_effect=interrupted):
                with self.assertRaises(OSError):
                    publish_firmware(image, '1.2.4', 124, key, public, output)
            self.assertEqual((output / 'latest.json').read_bytes(), raw)
            self.assertEqual((output / manifest['image']).read_bytes(), image_data)
            with self.assertRaisesRegex(ValueError, 'increase'):
                publish_firmware(image, '1.2.2', 122, key, public, output)
            image.write_bytes(image_data + b'x')
            with self.assertRaisesRegex(ValueError, 'immutable'):
                publish_firmware(image, '1.2.3', 125, key, public, output)
            self.assertEqual((output / 'latest.json').read_bytes(), raw)
            changed = dict(manifest, build=124)
            with self.assertRaises(ValueError):
                verify_firmware_manifest(json.dumps(changed).encode(), public)

    def test_manifest_rejects_duplicate_fields(self):
        with self.assertRaises(ValueError):
            verify_firmware_manifest(b'{"format":"x","format":"y"}',
                                     'sdk/publisher-public.pem')

    def test_filename_and_bounds(self):
        from passport.update import canonical_manifest
        digest = hashlib.sha256(b'x').hexdigest()
        for name in ('../app.bin', 'dir/app.bin', 'a?b'):
            with self.assertRaises(ValueError):
                canonical_manifest('1.0.0', 1, 4096, digest, name)


if __name__ == '__main__':
    unittest.main(verbosity=2)
