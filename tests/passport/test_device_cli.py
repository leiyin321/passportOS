import sys
import unittest
from pathlib import Path
sys.path.insert(0, 'sdk')
from passport.device import read_completion

class Port:
    def __init__(self, lines):
        self.lines = iter(lines)
        self.now = 0
    def readline(self):
        self.now += .2
        return next(self.lines, b'')
    def clock(self):
        return self.now

class DeviceCompletion(unittest.TestCase):
    def run_port(self, lines, **options):
        port = Port(lines)
        output = []
        read_completion(port, clock=port.clock, emit=output.append, **options)
        return output
    def test_missing_completion_is_failure(self):
        with self.assertRaisesRegex(ValueError, 'timeout'):
            self.run_port([b'I booting\n'])
    def test_fail_is_not_success(self):
        with self.assertRaisesRegex(ValueError, 'FAIL'):
            self.run_port([b'PASSPORT install FAIL signature\n', b'PASSPORT DONE\n'])
    def test_observe_keeps_worker_logs_after_ack(self):
        self.assertEqual(self.run_port([b'PASSPORT DONE\n', b'W passport_http no_memory\n'],
                                      verbose=True, observe=1),
                         ['PASSPORT DONE', 'W passport_http no_memory'])
    def test_default_stops_at_completion(self):
        self.assertEqual(self.run_port([b'I verbose\n', b'PASSPORT DONE\n']), ['PASSPORT DONE'])
    def test_catalog_permissions_match_manifest(self):
        import json
        catalog = json.loads(Path('protocol/catalog-v1.schema.json').read_text())
        manifest = json.loads(Path('protocol/manifest-v1.schema.json').read_text())
        allowed = catalog['properties']['apps']['items']['properties']['permissions']['items']['enum']
        self.assertEqual(set(allowed), set(manifest['properties']['permissions']['items']['enum']))

if __name__ == '__main__': unittest.main(verbosity=2)
