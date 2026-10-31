import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, "sdk")
from passport.declarative import validate_ui


class DeclarativePreflight(unittest.TestCase):
    def test_all_shipped_declarative_entries(self):
        from passport import unpack
        for path in Path("examples/packages").glob("*.pap"):
            package = path.read_bytes()
            manifest, _ = unpack(package, "sdk/publisher-public.pem")
            if manifest["runtime"] == "declarative":
                self.assertGreater(validate_ui(unpack(package, "sdk/publisher-public.pem")[1])["nodes"], 0)

    def test_malformed_runtime_contracts_are_rejected(self):
        valid = {"format": "passport.ui/1", "state": {},
                 "ui": [{"type": "text", "text": "Hello"}]}
        for document in (
            {**valid, "ui": [{"type": "unknown"}]},
            {**valid, "ui": [{"type": "icon"}]},
            {**valid, "actions": {"up": {"type": "unknown"}}},
            {**valid, "actions": {"ok": {"type": "fetch"}}},
            {**valid, "timer": {"period_ms": 100, "action": {"type": "save"}}},
            {**valid, "ui": [{"type": "row", "children": [] , "gap": 2}]},
        ):
            with self.subTest(document=document), self.assertRaises(ValueError):
                validate_ui(json.dumps(document))

    def test_bounds_and_duplicate_keys(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            validate_ui('{"format":"passport.ui/1","format":"passport.ui/1","state":{},"ui":[]}')
        with self.assertRaises(ValueError):
            validate_ui(json.dumps({"format": "passport.ui/1", "state": {},
                                    "ui": [{"type": "text"}] * 65}))
        with self.assertRaisesRegex(ValueError, "non-finite"):
            validate_ui('{"format":"passport.ui/1","state":{"n":1e999},"ui":[]}')

    def test_contract_reports_required_capabilities(self):
        result = validate_ui(json.dumps({
            "format": "passport.ui/1", "state": {"n": 0},
            "ui": [{"type": "text", "bind": "n"}],
            "actions": {"ok": {"type": "save"}},
        }))
        self.assertEqual(result["permissions"], {"ui", "input", "storage.private"})

    def test_package_preflight_catches_missing_permissions_and_assets(self):
        from passport import validate_entry
        manifest = {"runtime": "declarative", "permissions": ["ui", "graphics"], "domains": []}
        image = json.dumps({"format": "passport.ui/1", "state": {},
                            "ui": [{"type": "image", "src": "app://icons/missing"}]})
        with self.assertRaisesRegex(ValueError, "undeclared assets"):
            validate_entry(manifest, image.encode())
        manifest["permissions"] = ["ui"]
        with self.assertRaisesRegex(ValueError, "requires permissions"):
            validate_entry(manifest, image.encode())


if __name__ == "__main__":
    unittest.main(verbosity=2)
