"""The documentation gate must propagate failures and preserve profile settings."""

import io
import json
import shlex
import shutil
import sys
from contextlib import redirect_stdout
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "doxy"))
import GenerateDoxyfile as generator
import RunDoxygen as runner


class DoxygenTests(unittest.TestCase):
    def test_empty_navigation_slots_are_omitted_in_both_languages(self):
        with tempfile.TemporaryDirectory() as directory:
            configs = runner.generate_profiles(Path(directory), check_only=True)
            for config in configs:
                language = Path(config).suffix[1:]
                tree = ET.parse(Path(directory) / ("layout_" + language + ".xml"))
                tabs = tree.findall("./navindex//tab[@type='user']")
                self.assertTrue(tabs)
                self.assertTrue(all(tab.get("url", "").strip() for tab in tabs))
                self.assertTrue(all(tab.get("title", "").strip() for tab in tabs))
                self.assertIn("C++", [tab.get("title") for tab in tabs])

    def test_project_metadata_matches_profile_catalog(self):
        metadata = json.loads((runner.ROOT / "doxy/project.json").read_text(encoding="utf-8"))
        self.assertEqual(metadata["version"], 2)
        self.assertIn(metadata["default_profile"], metadata["profiles"])
        for name, path in metadata["profiles"].items():
            profile = runner.load_profile(runner.ROOT / "doxy" / path)
            self.assertEqual(profile.name, name)
            self.assertEqual(profile.visibility, "public")
        self.assertEqual((runner.ROOT / "doxy" / metadata["project_root"]).resolve(), runner.ROOT)
        self.assertTrue((runner.ROOT / "doxy" / metadata["run_entry"]).is_file())

    def test_profile_keys_do_not_overwrite_longer_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base, output = root / "base", root / "output"
            base.write_text("INPUT = original\nINPUT_ENCODING = UTF-8\n", encoding="utf-8")
            generator.generate_doxyfiles(str(base), [["Doxyfile", "0"], ["INPUT", "1"]],
                                         [[str(output), "api"]])
            self.assertEqual(output.read_text(encoding="utf-8"), "INPUT = api\nINPUT_ENCODING = UTF-8\n")

    def test_both_profiles_keep_warnings_enabled(self):
        with tempfile.TemporaryDirectory() as directory:
            configs = runner.generate_profiles(Path(directory), check_only=True)
            self.assertEqual({Path(p).suffix for p in configs}, {".en", ".ja"})
            for config in configs:
                settings = {}
                for line in Path(config).read_text(encoding="utf-8").splitlines():
                    if "=" in line and not line.lstrip().startswith("#"):
                        key, value = line.split("=", 1)
                        settings[key.strip()] = value.strip()
                self.assertEqual(settings["WARN_AS_ERROR"], "FAIL_ON_WARNINGS")
                self.assertEqual(settings["WARN_IF_DOC_ERROR"], "YES")
                self.assertEqual(settings["GENERATE_HTML"], "NO")
                # Doxygen rejects a run with every output format disabled.
                self.assertEqual(settings["GENERATE_XML"], "YES")
                self.assertIn("__declspec(x)=", settings["PREDEFINED"])
                self.assertTrue(Path(settings["LAYOUT_FILE"].strip('"')).is_file())

    def test_profiles_generate_with_non_utf8_console(self):
        # English Windows runners use cp1252 even when the input documents are Japanese.
        with tempfile.TemporaryDirectory(prefix="doxy_\u65e5\u672c_") as directory:
            with io.TextIOWrapper(io.BytesIO(), encoding="cp1252", errors="strict") as console:
                with redirect_stdout(console):
                    configs = runner.generate_profiles(Path(directory), check_only=True)
                self.assertEqual(len(configs), 2)
                self.assertTrue(all(Path(config).is_file() for config in configs))

    def test_failed_process_or_warnings_cannot_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "warnings.log"
            for exit_code, content, expected in [(1, "", False), (0, "warning: bad reference", False),
                                                  (0, "", True), (0, None, False)]:
                def run(*args, **kwargs):
                    if content is not None:
                        log.write_text(content, encoding="utf-8")
                    return mock.Mock(returncode=exit_code)
                log.write_text("", encoding="utf-8")
                with mock.patch.object(runner.subprocess, "run", side_effect=run):
                    self.assertEqual(runner.run_profile("doxygen", "config", log), expected)

    def test_legacy_batch_propagates_doxygen_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            batch = Path(directory) / "run.bat"
            generator.generate_run_batch(str(batch), ["doxyfile.ja", "doxyfile.en"])
            script = batch.read_text(encoding="utf-8")
            self.assertEqual(script.count("if errorlevel 1 (popd & exit /b 1)"), 2)

    @unittest.skipUnless(shutil.which("doxygen"), "Doxygen is not installed")
    def test_nested_checkout_does_not_resolve_a_parent_projects_markdown(self):
        with tempfile.TemporaryDirectory(prefix="doxy-nested-") as directory:
            parent = Path(directory).resolve()
            engine = parent / "engine/library"
            guide = engine / "doc/en/Guide.md"
            target = engine / "doxy/README.md"
            unrelated = parent / "doxy/README.md"
            for path in (guide, target, unrelated):
                path.parent.mkdir(parents=True, exist_ok=True)
            guide.write_text("# Guide\n\n[Tools](../../doxy/README.md)\n", encoding="utf-8")
            target.write_text("# Library tools\n", encoding="utf-8")
            unrelated.write_text("# Unrelated parent project\n", encoding="utf-8")
            output = engine / "doxy/generated/developer-public/check/lang_en"
            output.mkdir(parents=True)
            config = output / "Doxyfile"
            log = output / "warnings.log"
            config.write_text("\n".join([
                "INPUT = " + runner.quote(guide) + " " + runner.quote(target),
                "OUTPUT_DIRECTORY = " + runner.quote(output),
                "WARN_LOGFILE = " + runner.quote(log),
                "GENERATE_HTML = NO", "GENERATE_LATEX = NO", "GENERATE_XML = YES",
                "QUIET = YES", "WARN_AS_ERROR = FAIL_ON_WARNINGS", "HAVE_DOT = NO",
                "IMPLICIT_DIR_DOCS = NO",
            ]) + "\n", encoding="utf-8")
            with mock.patch.object(runner, "ROOT", engine):
                self.assertTrue(runner.run_profile(shutil.which("doxygen"), config, log),
                                log.read_text(encoding="utf-8") if log.exists() else "No warning log")


if __name__ == "__main__":
    unittest.main()
