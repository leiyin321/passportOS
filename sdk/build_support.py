"""Include the canonical C SDK in installed Python wheels."""
from pathlib import Path
import shutil
from setuptools.command.build_py import build_py

class BuildPy(build_py):
    def run(self):
        super().run()
        shutil.copytree(Path(__file__).parent/'native', Path(self.build_lib)/'passport/_native', dirs_exist_ok=True)
