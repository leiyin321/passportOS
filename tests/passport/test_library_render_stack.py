import unittest
from pathlib import Path


SOURCE = Path("main/passport_main.c").read_text()


def function_body(signature):
    start = SOURCE.index(signature)
    opening = SOURCE.index("{", start)
    depth = 1
    for index in range(opening + 1, len(SOURCE)):
        if SOURCE[index] == "{":
            depth += 1
        elif SOURCE[index] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[opening + 1:index]
    raise AssertionError(f"unterminated function: {signature}")


class LibraryRenderStack(unittest.TestCase):
    def test_package_metadata_is_loaded_before_card_render(self):
        refresh = function_body("static void refresh_app_list(void)")
        render_version = function_body("static void library_app_version(")
        draw = function_body("static void draw(void)")

        self.assertIn("installed_app_version(", refresh)
        self.assertIn("library_versions[index]", render_version)
        self.assertNotIn("installed_app_version(", render_version)
        self.assertNotIn("installed_app_version(", draw)


if __name__ == "__main__":
    unittest.main(verbosity=2)
