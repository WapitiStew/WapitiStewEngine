"""Regressions for translation identity and links outside the paired manifest."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CMAKE = shutil.which("cmake")


@unittest.skipUnless(CMAKE, "CMake is required for the documentation contract")
class DocumentationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="wse-doc-contract-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        manifest = (ROOT / "doc/DocumentationManifest.tsv").read_text(encoding="utf-8")
        rows = []
        for line in manifest.splitlines():
            if not line or line.startswith("#"):
                continue
            role, _, _, version, date = line.split("|")
            english, japanese = f"doc/en/{role}.md", f"doc/ja/{role}.md"
            rows.append("|".join((role, english, japanese, version, date)))
            metadata = f"> Documentation version: {version}\n> Last synchronized: {date}\n"
            self.write(english, "> Canonical language: English\n" + metadata)
            self.write(japanese, f"> Canonical source: [English](../en/{role}.md)\n" + metadata)
        self.write("doc/DocumentationManifest.tsv", "\n".join(rows) + "\n")
        for path in ("AGENTS.md", "doc/README.md", "doxy/README.md",
                     "example/cpp/core/quickstart.cpp", "example/python/core/quickstart.py",
                     "example/js/core/quickstart.js", "example/java/core/QuickStart.java",
                     "example/cs/core/QuickStart.cs"):
            self.write(path, "")
        # The fixture varies documentation only; use the actual build option and preset catalogs.
        for path in ("CMakePresets.json", "CMakeLists.txt", "cmake/WseOptions.cmake"):
            self.write(path, (ROOT / path).read_text(encoding="utf-8"))

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def run_contract(self):
        return subprocess.run(
            [CMAKE, f"-DWSE_SOURCE_ROOT={self.root.as_posix()}",
             f"-DWSE_DOCUMENTATION_MANIFEST={self.root.as_posix()}/doc/DocumentationManifest.tsv",
             "-P", str(ROOT / "test/cmake/verify_documentation.cmake")],
            capture_output=True, text=True, encoding="utf-8", errors="replace", check=False)

    def test_valid_pairs_and_unpaired_navigation_pass(self):
        self.write("doc/ja/SDK.md", "[Current overview](overview.md)\n")
        result = self.run_contract()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_existing_but_wrong_canonical_source_fails(self):
        path = self.root / "doc/ja/overview.md"
        self.write("doc/ja/overview.md", path.read_text(encoding="utf-8").replace(
            "../en/overview.md", "../en/build-guide.md"))
        result = self.run_contract()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Canonical source does not match manifest", result.stderr)

    def test_broken_link_in_unpaired_guide_fails(self):
        self.write("doc/ja/SDK.md", "[Missing guide](missing.md)\n")
        result = self.run_contract()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Broken local link in doc/ja/SDK.md", result.stderr)

    def test_required_design_role_cannot_disappear_from_manifest(self):
        manifest_path = self.root / "doc/DocumentationManifest.tsv"
        manifest = manifest_path.read_text(encoding="utf-8")
        for role in ("gef-file-formats", "core-algorithms", "core-services", "c-abi", "developer-walkthrough"):
            with self.subTest(role=role):
                self.write("doc/DocumentationManifest.tsv", "\n".join(
                    line for line in manifest.splitlines()
                    if not line.startswith(role + "|")) + "\n")
                result = self.run_contract()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "Required documentation role is not registered: " + role, result.stderr)


if __name__ == "__main__":
    unittest.main()
