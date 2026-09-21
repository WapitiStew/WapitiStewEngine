"""Regression tests for compiler diagnostics the warning gate must not miss."""

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "tools/ci/check_warning_ratchet.py"
SPEC = importlib.util.spec_from_file_location("warning_ratchet", SCRIPT)
RATCHET = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RATCHET)


class WarningRatchetTests(unittest.TestCase):
    def test_line_only_and_column_diagnostics_are_counted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = (f"{root}/api/example.h:12: warning: ignoring pragma [-Wunknown-pragmas]\n"
                   f"{root}/api/example.h:20:5: warning: unused value [-Wunused-variable]\n"
                   "api/../api/example.h:21: warning: another warning\n")
            self.assertEqual(RATCHET.count_warnings(log, root), {"api/example.h": 3})

    def test_external_and_generated_files_are_excluded(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "source"
            paths = [root / "vendor/dep.h", root / ".bootstrap-cache/tool.h",
                     root / "build/generated.h", root.parent / "system.h"]
            log = "".join(f"{path}:1: warning: ignored\n" for path in paths)
            self.assertEqual(RATCHET.count_warnings(log, root), {})

    def test_msvc_diagnostics_are_counted_with_or_without_columns(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = (f"{root}/api/a.h(12,4): warning C4251: example [project]\n"
                   f"1>{root}/api/a.h(19): warning C4996: example [project]\n"
                   f"{root}/api/a.h(20): note: context\n")
            self.assertEqual(RATCHET.count_warnings(log, root), {"api/a.h": 2})

    def test_windows_drive_letter_is_part_of_the_path(self):
        match = RATCHET.WARNING.search("C:\\source\\api\\a.h:12: warning: example\n")
        self.assertEqual(match.group("path"), "C:\\source\\api\\a.h")

    def test_line_only_warning_fails_an_empty_baseline(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "build.log"
            log.write_text("api/example.h:7: warning: regression\n", encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(SCRIPT), "--source-root", str(root),
                 "--log", str(log), "--baseline", str(root / "baseline.tsv")],
                capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 1)
            self.assertIn("api/example.h: 0 -> 1", result.stderr)


if __name__ == "__main__":
    unittest.main()
