"""Audience and visibility selection must isolate inputs, output and source listings."""

import json
import shlex
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "doxy"))
import RunDoxygen as runner


class ProfileTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="wse-doc-profiles-")
        self.addCleanup(temporary.cleanup)
        # Windows runners may expose TEMP through an 8.3 alias such as RUNNER~1.
        # Match the canonical paths returned by the documentation input resolver.
        self.base = Path(temporary.name).resolve()
        self.engine = self.base / "engine"
        self.overlay = self.base / "overlay"
        for root in (self.engine, self.overlay):
            for relative in ("api/Widget.h", "api/InlineSupport.cpp", "core/Implementation.cpp", "platform/Backend.cpp",
                             "test/NotAnInput.cpp", "vendor/NotAnInput.h", "doc/report/NotAnInput.md"):
                self.write(root / relative, "// fixture\n")
            for audience in ("user", "developer"):
                self.make_profile(root, audience, "public" if root == self.engine else "confidential")
        for relative in ("doxy/pages/user.en.md", "doxy/pages/user.ja.md", "tools/pi/README.md",
                         "doc/en/README.md", "doc/ja/README.md", "doc/design/en/Design.md",
                         "doc/design/ja/Design.md", "doxy/README.md"):
            self.write(self.engine / relative, "# Fixture\n")
        self.write(self.engine / "doxy/Style/header.html",
                   (runner.ROOT / "doxy/Style/header.html").read_text(encoding="utf-8"))
        patch = mock.patch.object(runner, "ROOT", self.engine)
        patch.start()
        self.addCleanup(patch.stop)

    @staticmethod
    def write(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def make_profile(self, root, audience, visibility):
        path = root / "doxy/profiles" / (audience + "-" + visibility + ".json")
        self.write(path, json.dumps(dict(version=1, audience=audience, visibility=visibility, source_root="../..")))
        return path

    def profile(self, audience, visibility):
        root = self.engine if visibility == "public" else self.overlay
        return runner.load_profile(root / "doxy/profiles" / (audience + "-" + visibility + ".json"))

    @staticmethod
    def settings(config):
        result = {}
        for line in Path(config).read_text(encoding="utf-8").splitlines():
            if "=" in line and not line.lstrip().startswith("#"):
                key, value = line.split("=", 1)
                result[key.strip()] = value.strip()
        return result

    def test_all_four_modes_have_disjoint_outputs_and_expected_inputs(self):
        outputs = set()
        for audience in ("user", "developer"):
            for visibility in ("public", "confidential"):
                profile = self.profile(audience, visibility)
                inputs = runner.input_files(profile, "ja")
                self.assertIn(self.engine / "api/Widget.h", inputs)
                self.assertEqual(self.overlay / "api/Widget.h" in inputs, visibility == "confidential")
                self.assertEqual(self.engine / "core/Implementation.cpp" in inputs, audience == "developer")
                self.assertEqual(self.engine / "api/InlineSupport.cpp" in inputs, audience == "developer")
                self.assertEqual(self.overlay / "platform/Backend.cpp" in inputs,
                                 audience == "developer" and visibility == "confidential")
                self.assertFalse(any("NotAnInput" in str(path) for path in inputs))
                for check in (False, True):
                    configs = runner.generate_profiles(profile.output(check) / "config", check, profile)
                    for config in configs:
                        settings = self.settings(config)
                        language = Path(config).suffix[1:]
                        self.assertEqual(shlex.split(settings["INPUT"]),
                                         [path.as_posix() for path in runner.input_files(profile, language)])
                        output = shlex.split(settings["OUTPUT_DIRECTORY"])[0]
                        self.assertEqual(shlex.split(settings["WARN_LOGFILE"])[0], output + "/warnings.log")
                        self.assertIn(visibility.title(), settings["PROJECT_NAME"])
                        self.assertNotIn(output, outputs)
                        outputs.add(output)
                        self.assertTrue(runner.within(Path(output), profile.source_root))
                        for key in ("EXTRACT_PRIVATE", "INTERNAL_DOCS", "SOURCE_BROWSER", "INLINE_SOURCES",
                                    "VERBATIM_HEADERS", "XML_PROGRAMLISTING"):
                            self.assertEqual(settings[key], "YES" if audience == "developer" else "NO")
                        self.assertEqual(settings["WARN_AS_ERROR"], "FAIL_ON_WARNINGS")
                        self.assertEqual(settings["CASE_SENSE_NAMES"], "NO")
                        self.assertEqual(settings["GENERATE_XML"], "YES" if check or audience == "developer" else "NO")
                        for key in ("PROJECT_LOGO", "PROJECT_ICON", "HTML_HEADER", "HTML_FOOTER",
                                    "HTML_EXTRA_STYLESHEET", "HTML_EXTRA_FILES"):
                            self.assertTrue(Path(shlex.split(settings[key])[0]).is_absolute())
        self.assertEqual(len(outputs), 16)

    def test_public_profile_cannot_select_an_overlay(self):
        path = self.make_profile(self.overlay, "user", "public")
        with self.assertRaisesRegex(ValueError, "only the engine"):
            runner.load_profile(path)

    def test_confidential_profile_cannot_belong_to_public_tree(self):
        path = self.make_profile(self.engine, "user", "confidential")
        with self.assertRaisesRegex(ValueError, "outside the public"):
            runner.load_profile(path)

    def test_missing_overlay_does_not_fall_back_to_public(self):
        with self.assertRaises(FileNotFoundError):
            runner.load_profile(self.base / "missing/profiles/user-confidential.json")

    def test_public_submodule_under_overlay_external_directory_is_isolated(self):
        engine = self.overlay / "external/Engine"
        self.write(engine / "api/Public.h", "// public\n")
        self.write(engine / "doxy/pages/user.en.md", "# Public API\n")
        with mock.patch.object(runner, "ROOT", engine):
            profile = self.profile("user", "confidential")
            inputs = runner.input_files(profile, "en")
            self.assertIn(engine / "api/Public.h", inputs)
            self.assertIn(self.overlay / "api/Widget.h", inputs)
            self.assertFalse(runner.within(profile.output(), engine))
            public_profile = self.make_profile(engine, "user", "public")
            public_inputs = runner.input_files(runner.load_profile(public_profile), "en")
            self.assertNotIn(self.overlay / "api/Widget.h", public_inputs)

    def test_public_checkout_cannot_overlap_controlled_inputs_or_output(self):
        for relative in ("api/Engine", "core/Engine", "platform/Engine", "doc/Engine",
                         "doxy/generated/user-confidential/full/Engine"):
            with mock.patch.object(runner, "ROOT", self.overlay / relative):
                with self.assertRaises(ValueError):
                    self.profile("user", "confidential")

    def test_confidential_configs_cannot_be_written_in_public_tree(self):
        with self.assertRaisesRegex(ValueError, "outside the public"):
            runner.generate_profiles(self.engine / "doxy/generated/config", profile=self.profile("user", "confidential"))

    def test_single_language_does_not_link_to_unbuilt_translation(self):
        profile = self.profile("user", "public")
        directory = profile.output() / "config"
        configs = runner.generate_profiles(directory, profile=profile, languages=("en",))
        self.assertEqual([Path(p).suffix for p in configs], [".en"])
        header = (directory / "header.html").read_text(encoding="utf-8")
        self.assertIn('data-lang="en"', header)
        self.assertNotIn('data-lang="ja"', header)

    def test_configuration_injection_is_rejected(self):
        for value in ('a"\nINPUT = elsewhere', "a\nb"):
            with self.assertRaises(ValueError):
                runner.quote(value)

    def test_user_site_removes_stale_xml_but_preserves_rendered_outputs(self):
        profile = self.profile("user", "public")
        output = profile.output() / "lang_en"
        self.write(output / "xml/private.xml", "raw model")
        self.write(output / "html/index.html", "API reference")
        runner.generate_profiles(profile.output() / "config", profile=profile, languages=("en",))
        self.assertTrue((output / "xml/private.xml").exists(), "Generating configuration must not remove output")
        runner.remove_stale_user_xml(profile, output)
        self.assertFalse((output / "xml").exists())
        self.assertEqual((output / "html/index.html").read_text(), "API reference")

    def create_link(self, link, target, directory=False):
        link.parent.mkdir(parents=True, exist_ok=True)
        try:
            link.symlink_to(target, target_is_directory=directory)
        except OSError:
            self.skipTest("Creating symlinks is not permitted on this host")

    def test_api_symlink_cannot_import_overlay_into_public_output(self):
        self.create_link(self.engine / "api/Linked.h", self.overlay / "api/Widget.h")
        with self.assertRaisesRegex(ValueError, "input escapes"):
            runner.input_files(self.profile("user", "public"), "en")

    def test_stale_xml_cleanup_cannot_follow_a_link_outside_output(self):
        profile = self.profile("user", "public")
        output = profile.output() / "lang_en"
        target = self.overlay / "keep"
        self.write(target / "private.xml", "retain")
        self.create_link(output / "xml", target, directory=True)
        with self.assertRaisesRegex(ValueError, "escapes"):
            runner.remove_stale_user_xml(profile, output)
        self.assertEqual((target / "private.xml").read_text(), "retain")

    def test_language_output_symlink_cannot_export_confidential_data(self):
        profile = self.profile("user", "confidential")
        target = self.engine / "generated-cross"
        target.mkdir()
        self.create_link(profile.output() / "lang_en", target, directory=True)
        with self.assertRaisesRegex(ValueError, "outside the public"):
            runner.generate_profiles(profile.output() / "config", profile=profile)
        self.assertEqual(list(target.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
