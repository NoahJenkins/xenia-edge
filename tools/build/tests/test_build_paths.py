"""Regression tests for finding executables produced by CMake."""

import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[3] / "xenia-build.py"
SPEC = importlib.util.spec_from_file_location("xenia_build", SCRIPT)
BUILD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD)


class BuildPathsTest(unittest.TestCase):
    def test_test_runner_finds_cmake_output_on_each_platform(self):
        for host, output in (("darwin", "macOS"), ("win32", "Windows"),
                             ("linux", "Linux")):
            with self.subTest(host=host), tempfile.TemporaryDirectory() as root:
                binary = Path(root) / "build" / "bin" / output / "Checked" / "portal-tests"
                binary.parent.mkdir(parents=True)
                binary.write_text("test executable placeholder\n")
                binary.chmod(0o700)
                with patch.object(BUILD, "self_path", root), patch.object(BUILD.sys, "platform", host):
                    folder = BUILD.get_build_bin_path({"config": "checked"})
                    self.assertEqual(BUILD.get_bin(os.path.join(folder, "portal-tests")), str(binary))


if __name__ == "__main__":
    unittest.main()
