"""Exercise installed license attribution and mixed-package metadata without compiling code."""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class LicensingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.build = self.root / "build"
        self.prefix = self.root / "install"
        for name in ("LICENSE", "NOTICE"):
            shutil.copyfile(ROOT / name, self.source / name)

    def run_command(self, *args):
        return subprocess.run(args, capture_output=True, text=True, encoding="utf-8", errors="replace")

    def package(self, extension=False, missing_terms=False):
        setup = ""
        if extension:
            setup = 'set(WSE_EXTENSION_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")\n'
            if not missing_terms:
                (self.source / "overlay-license").write_text("Example overlay terms.\n", encoding="utf-8")
                setup += ('set(WSE_EXTENSION_LICENSE_IDENTIFIER "LicenseRef-Example-Overlay")\n'
                          'set(WSE_EXTENSION_LICENSE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/overlay-license")\n')
        cmake = ('cmake_minimum_required(VERSION 3.24)\nproject(LicenseFixture NONE)\n'
                 + setup + 'include("' + (ROOT / "cmake/WseLicensing.cmake").as_posix() + '")\n'
                 + 'configure_file(manifest.in wse-package.json @ONLY)\n'
                 + 'install(FILES "${CMAKE_CURRENT_BINARY_DIR}/wse-package.json" DESTINATION .)\n')
        (self.source / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
        (self.source / "manifest.in").write_text(
            '{"version":"1.0.0","license":"@WSE_PACKAGE_LICENSE_EXPRESSION@",'
            '"license_files":[@WSE_PACKAGE_LICENSE_FILES_JSON@],"extension_commit":"'
            + ("example-revision" if extension else "none") + '"}', encoding="utf-8")
        result = self.run_command("cmake", "-S", str(self.source), "-B", str(self.build))
        if missing_terms:
            self.assertNotEqual(0, result.returncode)
            self.assertIn("requires its own license", result.stderr)
            return
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        result = self.run_command("cmake", "--install", str(self.build), "--prefix", str(self.prefix))
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def metadata(self, succeeds=True):
        result = self.run_command(sys.executable, str(ROOT / "tools/release/make_release_metadata.py"),
                                  "--package-root", str(self.prefix))
        if succeeds:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            return json.loads((self.prefix / "sbom.cdx.json").read_text(encoding="utf-8"))
        self.assertNotEqual(0, result.returncode)
        self.assertFalse((self.prefix / "sbom.cdx.json").exists())

    def test_public_install_and_sbom_preserve_license_and_dependency_attribution(self):
        self.package()
        for name in ("LICENSE", "NOTICE"):
            self.assertEqual((ROOT / name).read_bytes(), (self.prefix / name).read_bytes())
        dependency = self.prefix / "vendor/example"
        dependency.mkdir(parents=True)
        (dependency / "COPYING").write_text("Example third-party terms.", encoding="utf-8")
        (dependency / "wse-dependency.json").write_text(json.dumps(
            {"name": "example", "license": "MIT", "license_file": "COPYING"}), encoding="utf-8")
        sbom = self.metadata()
        self.assertEqual([{"expression": "Apache-2.0"}], sbom["metadata"]["component"]["licenses"])
        self.assertEqual([{"license": {"name": "MIT"}}], sbom["components"][0]["licenses"])
        self.assertIn("vendor/example/COPYING", (self.prefix / "THIRD_PARTY_NOTICES.md").read_text())
        self.assertFalse((self.prefix / "LICENSE.extension").exists())

    def test_overlay_retains_both_terms_in_install_and_sbom(self):
        self.package(extension=True)
        self.assertEqual("Example overlay terms.\n", (self.prefix / "LICENSE.extension").read_text())
        self.assertEqual((ROOT / "LICENSE").read_bytes(), (self.prefix / "LICENSE").read_bytes())
        self.assertEqual([{"expression": "Apache-2.0 AND LicenseRef-Example-Overlay"}],
                         self.metadata()["metadata"]["component"]["licenses"])

    def test_overlay_without_terms_fails_configure(self):
        self.package(extension=True, missing_terms=True)

    def test_missing_notice_rejects_metadata(self):
        self.package()
        (self.prefix / "NOTICE").unlink()
        self.metadata(succeeds=False)

    def test_overlay_cannot_be_reported_as_apache_only(self):
        self.package(extension=True)
        path = self.prefix / "wse-package.json"
        manifest = json.loads(path.read_text())
        manifest["license"] = "Apache-2.0"
        path.write_text(json.dumps(manifest), encoding="utf-8")
        self.metadata(succeeds=False)

    def test_missing_overlay_notice_rejects_metadata(self):
        self.package(extension=True)
        (self.prefix / "LICENSE.extension").unlink()
        self.metadata(succeeds=False)

    def test_old_manifest_cannot_be_silently_relicensed(self):
        self.package()
        (self.prefix / "wse-package.json").write_text('{"version":"1.0.0"}', encoding="utf-8")
        self.metadata(succeeds=False)


if __name__ == "__main__":
    unittest.main()
