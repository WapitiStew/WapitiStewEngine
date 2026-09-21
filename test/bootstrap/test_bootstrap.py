import hashlib
import importlib.util
import io
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock


WSE_ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("wse_standalone_bootstrap", WSE_ROOT / "bootstrap.py")
BOOTSTRAP = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(BOOTSTRAP)


class StandaloneBootstrapTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "Windows launcher contract")
    def test_windows_launcher_propagates_bootstrap_failure(self):
        completed = subprocess.run(
            [
                os.environ.get("COMSPEC", "cmd.exe"),
                "/d",
                "/c",
                str(WSE_ROOT / "bootstrap.bat"),
                "--package",
                "wse-intentionally-unknown-package",
            ],
            cwd=WSE_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertEqual(completed.returncode, 2, completed.stdout + completed.stderr)
        self.assertIn("Unknown package name", completed.stdout + completed.stderr)

    def test_repository_manifest_pins_version_license_and_source_identity(self):
        manifest = json.loads((WSE_ROOT / "bootstrap-manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["schema_version"], 2)
        self.assertEqual(manifest["cache_format"], "wse-portable-cache-v2")
        packages = [package for package in manifest["packages"] if package.get("role") == "dependency"]
        self.assertEqual(
            {package["name"] for package in packages},
            {
                "libjpeg-turbo",
                "openssl",
                "libcurl",
                "libcamera",
                "nlohmann-json",
                "pybind11",
                "node-api-headers",
            },
        )
        for package in packages:
            self.assertTrue(package["version"])
            self.assertTrue(package["license"])
            self.assertTrue(package["install_subdir"])
            self.assertTrue(package["required_files"])
            if package.get("source", {}).get("type") == "git":
                self.assertRegex(package["source"]["commit"], r"^[0-9a-f]{40}$")
            else:
                self.assertRegex(package["sha256"], r"^[0-9a-f]{64}$")

        libcurl = next(package for package in packages if package["name"] == "libcurl")
        self.assertEqual(libcurl["license"], "curl")
        self.assertEqual(libcurl["metadata_file"], "wse-dependency.json")
        self.assertEqual(set(libcurl["platforms"]), {"windows", "linux"})
        self.assertNotEqual(
            libcurl["platforms"]["windows"]["config"]["extracted_dir"],
            libcurl["platforms"]["linux"]["config"]["extracted_dir"],
        )
        self.assertRegex(libcurl["signature"]["signer_fingerprint"], r"^[0-9A-F]{40}$")
        self.assertEqual(
            libcurl["signature"]["file"],
            f"{libcurl['archive_name']}.asc",
        )
        linux_commands = libcurl["platforms"]["linux"]["config"]["build"]["commands"]
        linux_configures = [command for command in linux_commands if "-S" in command]
        self.assertTrue(linux_configures)
        for command in linux_configures:
            self.assertIn("-DCMAKE_POSITION_INDEPENDENT_CODE=ON", command)

        openssl = next(package for package in packages if package["name"] == "openssl")
        self.assertFalse(openssl["default"])
        self.assertEqual(set(openssl["platforms"]), {"linux"})
        self.assertEqual(
            set(openssl["platforms"]["linux"]["architectures"]),
            {"arm64"},
        )
        with mock.patch.object(BOOTSTRAP, "_host_platform", return_value="linux"):
            resolved_openssl = BOOTSTRAP._resolve_platform(openssl, "arm64")
        self.assertEqual(resolved_openssl["resolved_architecture"], "arm64")
        self.assertEqual(resolved_openssl["install_subdir"], "openssl-arm64")

        selected_by_default = BOOTSTRAP._selected_packages(manifest, [])
        self.assertNotIn("openssl", {package["name"] for package in selected_by_default})

        libcamera = next(package for package in packages if package["name"] == "libcamera")
        self.assertFalse(libcamera["default"])
        self.assertEqual(libcamera["version"], "0.7.2")
        self.assertEqual(libcamera["license"], "LGPL-2.1-or-later")
        self.assertEqual(
            libcamera["source"]["commit"],
            "191e202178f02430b5942397c70d215cdd2056fa",
        )
        self.assertEqual(set(libcamera["platforms"]), {"linux"})
        with mock.patch.object(BOOTSTRAP, "_host_platform", return_value="linux"):
            resolved_libcamera = BOOTSTRAP._resolve_platform(libcamera, "x86_64")
        self.assertEqual(resolved_libcamera["resolved_architecture"], "x86_64")
        self.assertEqual(resolved_libcamera["linkage"], "shared")
        self.assertNotIn("libcamera", {package["name"] for package in selected_by_default})

        node_api = next(package for package in packages if package["name"] == "node-api-headers")
        self.assertFalse(node_api["default"])
        self.assertEqual(node_api["version"], "24.19.0")
        self.assertEqual(node_api["license"], "MIT")
        self.assertEqual(node_api["linkage"], "header-only")
        self.assertEqual(len(node_api["supplemental_files"]), 1)
        self.assertRegex(node_api["supplemental_files"][0]["sha256"], r"^[0-9a-f]{64}$")
        self.assertEqual(node_api["supplemental_files"][0]["target"], "LICENSE")
        self.assertNotIn(
            "node-api-headers", {package["name"] for package in selected_by_default}
        )

        pybind11 = next(package for package in packages if package["name"] == "pybind11")
        self.assertFalse(pybind11["default"])
        self.assertEqual(pybind11["version"], "3.1.0")
        self.assertEqual(pybind11["license"], "BSD-3-Clause")
        self.assertEqual(pybind11["linkage"], "header-only")
        self.assertNotIn("pybind11", {package["name"] for package in selected_by_default})

        libjpeg_turbo = next(
            package for package in packages if package["name"] == "libjpeg-turbo"
        )
        self.assertEqual(libjpeg_turbo["install_subdir"], "vpj/libjpeg-turbo")
        self.assertEqual(
            libjpeg_turbo["platforms"]["linux"]["install_subdir"],
            "vpj/libjpeg-turbo-linux",
        )
        self.assertEqual(
            libjpeg_turbo["platforms"]["linux"]["architectures"]["arm64"][
                "install_subdir"
            ],
            "vpj/libjpeg-turbo-arm64",
        )

        # The build definition spans the root file and its responsibility modules under cmake/.
        cmake_source = (WSE_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        for module in sorted((WSE_ROOT / "cmake").glob("Wse*.cmake")):
            cmake_source += module.read_text(encoding="utf-8")
        self.assertNotIn("WSE_LIBJPEG_TURBO_ROOT", cmake_source)
        self.assertNotIn("WSE_LEGACY_VENDOR_JPEG_DIR", cmake_source)
        self.assertIn("${WSE_DEPENDENCY_ROOT}/vpj/libjpeg-turbo", cmake_source)
        package_template = (WSE_ROOT / "cmake/wse-package.json.in").read_text(encoding="utf-8")
        for variable, expected in (
            ("WSE_LIBCURL_BOOTSTRAP_VERSION", libcurl["version"]),
            ("WSE_LIBCURL_BOOTSTRAP_LICENSE", libcurl["license"]),
            ("WSE_LIBCURL_BOOTSTRAP_SHA256", libcurl["sha256"]),
        ):
            match = re.search(rf'set\({variable} "([^"]+)"\)', cmake_source)
            self.assertIsNotNone(match)
            self.assertEqual(match.group(1), expected)
            self.assertIn(f"@{variable}@", package_template)
        # libjpeg-turbo backs the controlled extension only, so its pins reach the manifest
        # through the CMake fragment emitted for extension builds; the public template carries
        # the placeholder, never the dependency itself.
        for variable, expected in (
            ("WSE_LIBJPEG_TURBO_BOOTSTRAP_VERSION", libjpeg_turbo["version"]),
            ("WSE_LIBJPEG_TURBO_BOOTSTRAP_LICENSE", libjpeg_turbo["license"]),
            ("WSE_LIBJPEG_TURBO_BOOTSTRAP_SHA256", libjpeg_turbo["sha256"]),
        ):
            match = re.search(rf'set\({variable} "([^"]+)"\)', cmake_source)
            self.assertIsNotNone(match)
            self.assertEqual(match.group(1), expected)
            self.assertIn(f"${{{variable}}}", cmake_source)
            self.assertNotIn(f"@{variable}@", package_template)
        self.assertIn("@WSE_PACKAGE_CONTROLLED_DEPENDENCY_JSON@", package_template)
        self.assertIn('"integration_status": "xpt-http-adapter"', package_template)
        self.assertNotIn("cpprest", package_template.lower())

        public_config_template = (
            WSE_ROOT / "cmake/WonderStewEngineConfig.cmake.in"
        ).read_text(encoding="utf-8")
        self.assertNotIn("cpprest", public_config_template.lower())

    def test_offline_missing_archive_never_opens_a_url(self):
        package = {
            "name": "sample",
            "version": "1.0.0",
            "install_subdir": "sample",
            "required_files": ["include/sample.h"],
            "archive_name": "sample.zip",
            "sha256": "0" * 64,
            "config": {"extracted_dir": "sample-extract", "build_working_dir": "sample", "copy_rules": []},
        }
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            BOOTSTRAP.urllib.request, "urlopen"
        ) as urlopen:
            root = Path(temporary)
            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Offline cache is missing sample 1.0.0"):
                BOOTSTRAP.provision_package(package, root / "cache", root / "vendor", offline=True)
            urlopen.assert_not_called()

    def test_offline_missing_supplemental_file_never_opens_a_url(self):
        package = {
            "name": "sample",
            "version": "1.0.0",
            "install_subdir": "sample",
            "required_files": ["include/sample.h", "LICENSE"],
            "archive_name": "sample.zip",
            "sha256": "0" * 64,
            "supplemental_files": [
                {
                    "url": "https://example.invalid/LICENSE",
                    "file": "sample-LICENSE",
                    "sha256": "1" * 64,
                    "target": "LICENSE",
                }
            ],
            "config": {
                "extracted_dir": "sample-extract",
                "build_working_dir": "sample",
                "copy_rules": [],
            },
        }
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            BOOTSTRAP.urllib.request, "urlopen"
        ) as urlopen:
            root = Path(temporary)
            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "supplemental file"):
                BOOTSTRAP.provision_package(
                    package, root / "cache", root / "vendor", offline=True
                )
            urlopen.assert_not_called()

    def test_offline_missing_git_cache_never_runs_clone(self):
        commit = "b" * 40
        package = {
            "name": "sample-git",
            "version": "1.0.0",
            "install_subdir": "sample-git",
            "required_files": ["include/sample.h"],
            "source": {
                "type": "git",
                "url": "https://example.invalid/sample.git",
                "checkout_dir": "sample-git-1.0.0",
                "bundle": "sample-git-1.0.0.bundle",
                "commit": commit,
            },
            "config": {"build_working_dir": ".", "copy_rules": []},
        }
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            BOOTSTRAP.shutil, "which", return_value="git"
        ), mock.patch.object(BOOTSTRAP, "_run") as run:
            root = Path(temporary)
            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Offline cache is missing sample-git 1.0.0"):
                BOOTSTRAP.provision_package(package, root / "cache", root / "vendor", offline=True)
            run.assert_not_called()

    def test_offline_missing_signature_never_opens_a_url(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            BOOTSTRAP.urllib.request, "urlopen"
        ) as urlopen:
            cache = Path(temporary)
            archive = cache / "sample.tar.xz"
            archive.write_bytes(b"pinned archive")
            package = {
                "name": "sample",
                "version": "1.0.0",
                "archive_name": archive.name,
                "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
                "signature": {
                    "file": "sample.tar.xz.asc",
                    "url": "https://example.invalid/sample.tar.xz.asc",
                },
            }

            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "missing the release signature"):
                BOOTSTRAP.verify_cache(package, cache)
            urlopen.assert_not_called()

    def test_non_ascii_signature_reports_bootstrap_error(self):
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary)
            archive = cache / "sample.tar.xz"
            archive.write_bytes(b"pinned archive")
            (cache / "sample.tar.xz.asc").write_bytes(b"\xff\xfe")
            package = {
                "name": "sample",
                "version": "1.0.0",
                "archive_name": archive.name,
                "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
                "signature": {"file": "sample.tar.xz.asc"},
            }

            with self.assertRaisesRegex(
                BOOTSTRAP.BootstrapError,
                "Cannot read cached release signature as ASCII",
            ):
                BOOTSTRAP.verify_cache(package, cache)

    def test_offline_archive_installs_transactionally_from_cache(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "cache"
            cache.mkdir()
            archive = cache / "sample.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("sample/include/sample.h", "header")
                output.writestr("sample/LICENSE", "license")
            package = {
                "name": "sample",
                "version": "1.0.0",
                "license": "MIT",
                "license_file": "LICENSE",
                "install_subdir": "owner/sample",
                "linkage": "static",
                "metadata_file": "wse-dependency.json",
                "required_files": ["include/sample.h", "LICENSE"],
                "archive_name": archive.name,
                "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
                "config": {
                    "extracted_dir": "sample-extract",
                    "build_working_dir": "sample",
                    "copy_rules": [
                        {"from": "sample/include", "to": "include", "type": "dir"},
                        {"from": "sample/LICENSE", "to": "LICENSE", "type": "file"},
                    ],
                },
            }

            result = BOOTSTRAP.provision_package(package, cache, root / "vendor", offline=True)

            self.assertIn("installed", result)
            self.assertEqual(
                (root / "vendor/owner/sample/include/sample.h").read_text(), "header"
            )
            metadata = json.loads(
                (root / "vendor/owner/sample/wse-dependency.json").read_text()
            )
            self.assertEqual(metadata["name"], "sample")
            self.assertEqual(metadata["target"]["platform"], BOOTSTRAP._host_platform())
            self.assertEqual(metadata["schema_version"], 2)
            self.assertRegex(metadata["recipe_sha256"], r"^[0-9a-f]{64}$")
            self.assertFalse(list(root.glob(".wse-sample-*")))
            self.assertFalse(list((root / "vendor/owner").glob(".wse-sample-*")))

    def test_tar_xz_is_extracted_without_external_tools(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "sample.tar.xz"
            payload = b"header"
            with tarfile.open(archive, "w:xz") as output:
                member = tarfile.TarInfo("sample/include/sample.h")
                member.size = len(payload)
                output.addfile(member, io.BytesIO(payload))

            destination = root / "extract"
            BOOTSTRAP._safe_extract_archive(archive, destination)

            self.assertEqual((destination / "sample/include/sample.h").read_bytes(), payload)

    @unittest.skipUnless(os.name == "nt", "Windows tar extraction path")
    def test_a_normal_length_tar_uses_a_plain_windows_destination(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "sample.tar.xz"
            payload = b"header"
            with tarfile.open(archive, "w:xz") as output:
                member = tarfile.TarInfo("sample/include/sample.h")
                member.size = len(payload)
                output.addfile(member, io.BytesIO(payload))

            destination = root / "extract"
            self.assertFalse(
                BOOTSTRAP._requires_extended_length_path(
                    destination,
                    ["sample/include/sample.h"],
                )
            )
            BOOTSTRAP._safe_extract_archive(archive, destination)

            self.assertEqual((destination / "sample/include/sample.h").read_bytes(), payload)

    @staticmethod
    def _write_link_archive(path: Path, link_name: str, link_target: str) -> None:
        """A two-member tar: a real file first, then a symbolic link that points at it."""
        payload = b"payload"
        with tarfile.open(path, "w:gz") as output:
            member = tarfile.TarInfo("pkg/lib/real.txt")
            member.size = len(payload)
            output.addfile(member, io.BytesIO(payload))
            link = tarfile.TarInfo(link_name)
            link.type = tarfile.SYMTYPE
            link.linkname = link_target
            output.addfile(link)

    def test_a_read_only_extracted_tree_can_still_be_replaced(self):
        # A Linux archive stores mode 0444 for its licence files, which Windows materializes as the
        # read-only attribute. Replacing a cached toolchain must not trip over it.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "readonly.tar.gz"
            payload = b"licence"
            with tarfile.open(archive, "w:gz") as output:
                member = tarfile.TarInfo("pkg/legal/LICENSE")
                member.size = len(payload)
                member.mode = 0o444
                output.addfile(member, io.BytesIO(payload))

            destination = root / "out"
            BOOTSTRAP._safe_extract_archive(archive, destination)
            self.assertEqual(
                (destination / "pkg/legal/LICENSE").read_text(encoding="utf-8"), "licence"
            )

            BOOTSTRAP._remove_tree(destination)
            self.assertFalse(destination.exists())

    def test_a_dependency_archive_still_rejects_every_link(self):
        # A dependency is copied into vendor/ and redistributed, so it must be a plain file tree.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "linked.tar.gz"
            self._write_link_archive(archive, "pkg/bin/tool", "../lib/real.txt")

            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Unsupported archive member"):
                BOOTSTRAP._safe_extract_archive(archive, root / "strict")

    def test_a_toolchain_archive_extracts_an_internal_link(self):
        # Node.js points `bin/npm` into `lib/node_modules` and a JDK points its per-module licence
        # files at a shared copy, so refusing every link made the pinned Linux archives unusable.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "linked.tar.gz"
            self._write_link_archive(archive, "pkg/bin/tool", "../lib/real.txt")

            destination = root / "relaxed"
            BOOTSTRAP._safe_extract_archive(archive, destination, allow_internal_links=True)

            # A host that cannot create a symbolic link gets the target's content copied instead;
            # either outcome must leave the documented path readable.
            self.assertEqual(
                (destination / "pkg/bin/tool").read_text(encoding="utf-8"), "payload"
            )

    def test_a_link_that_escapes_the_destination_is_rejected_even_when_links_are_allowed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "escaping.tar.gz"
            self._write_link_archive(archive, "pkg/bin/tool", "../../../escaped.txt")

            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Unsafe archive member"):
                BOOTSTRAP._safe_extract_archive(archive, root / "out", allow_internal_links=True)
            self.assertFalse((root / "escaped.txt").exists())

    def test_an_absolute_link_target_is_rejected_even_when_links_are_allowed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "absolute.tar.gz"
            self._write_link_archive(archive, "pkg/bin/tool", "/etc/shadow")

            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Absolute link target"):
                BOOTSTRAP._safe_extract_archive(archive, root / "out", allow_internal_links=True)

    def test_a_tar_that_names_its_root_as_dot_is_extracted(self):
        # The .NET SDK tarballs start with a `./` member. An extended-length Windows path is used
        # literally, so that member has to be normalized away rather than written as a directory
        # called `.`.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "dotroot.tar.gz"
            payload = b"payload"
            with tarfile.open(archive, "w:gz") as output:
                for name in ("./", "./pkg/"):
                    directory = tarfile.TarInfo(name)
                    directory.type = tarfile.DIRTYPE
                    # TarInfo defaults to 0o644, which leaves a directory a non-root user cannot
                    # enter; real archives store 0o755.
                    directory.mode = 0o755
                    output.addfile(directory)
                member = tarfile.TarInfo("./pkg/file.txt")
                member.size = len(payload)
                output.addfile(member, io.BytesIO(payload))

            destination = root / "out"
            BOOTSTRAP._safe_extract_archive(archive, destination)

            self.assertEqual((destination / "pkg/file.txt").read_text(encoding="utf-8"), "payload")
            self.assertFalse((destination / ".").is_symlink())
            self.assertEqual(sorted(item.name for item in destination.iterdir()), ["pkg"])

    @unittest.skipUnless(os.name == "nt", "Windows path length limit")
    def test_an_archive_member_longer_than_the_windows_limit_is_extracted(self):
        # The pinned .NET SDK contains member paths past 260 characters, so extraction has to write
        # through the extended-length prefix instead of failing partway through.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            member = "/".join(["wse-long-path-segment"] * 12) + "/payload.txt"
            archive = root / "deep.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr(member, "payload")

            destination = root / "extract"
            self.assertGreater(len(str(destination / member)), 260)
            BOOTSTRAP._safe_extract_archive(archive, destination)

            extracted = BOOTSTRAP._extended_length_path(destination) / member
            self.assertEqual(extracted.read_text(encoding="utf-8"), "payload")

            # Replacing a cached toolchain deletes the previous tree, which hits the same limit.
            BOOTSTRAP._remove_tree(destination)
            self.assertFalse(destination.exists())

    def test_an_archive_member_that_escapes_the_destination_is_rejected(self):
        # Containment is still checked against the plain path, so the long-path prefix cannot be
        # used to smuggle a traversal past the guard.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "escape.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("../escaped.txt", "payload")

            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Unsafe archive member"):
                BOOTSTRAP._safe_extract_archive(archive, root / "extract")
            self.assertFalse((root / "escaped.txt").exists())

    def test_zip_symbolic_link_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "sample.zip"
            member = zipfile.ZipInfo("sample/link")
            member.create_system = 3
            member.external_attr = (stat.S_IFLNK | 0o777) << 16
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr(member, "target")

            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Unsupported archive member"):
                BOOTSTRAP._safe_extract_archive(archive, root / "extract")

    def test_dependency_metadata_mismatch_rejects_existing_target(self):
        package = {
            "name": "sample",
            "version": "1.0.0",
            "license": "MIT",
            "license_file": "LICENSE",
            "install_subdir": "sample",
            "linkage": "static",
            "metadata_file": "wse-dependency.json",
            "required_files": ["include/sample.h", "LICENSE"],
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / "vendor/sample"
            (target / "include").mkdir(parents=True)
            (target / "include/sample.h").write_text("header", encoding="utf-8")
            (target / "LICENSE").write_text("license", encoding="utf-8")
            (target / "wse-dependency.json").write_text("{}", encoding="utf-8")

            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "identity mismatch"):
                BOOTSTRAP.provision_package(package, root / "cache", root / "vendor", offline=True)

    def test_dependency_metadata_locks_build_recipe(self):
        package = {
            "name": "sample",
            "version": "1.0.0",
            "license": "MIT",
            "license_file": "LICENSE",
            "install_subdir": "sample",
            "linkage": "static",
            "metadata_file": "wse-dependency.json",
            "required_files": ["include/sample.h", "LICENSE"],
            "config": {"build": {"commands": [["cmake", "--build", "first"]]}},
        }
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "vendor/sample"
            (target / "include").mkdir(parents=True)
            (target / "include/sample.h").write_text("header", encoding="utf-8")
            (target / "LICENSE").write_text("license", encoding="utf-8")
            (target / "wse-dependency.json").write_text(
                json.dumps(BOOTSTRAP._dependency_metadata(package)),
                encoding="utf-8",
            )

            changed = dict(package)
            changed["config"] = {"build": {"commands": [["cmake", "--build", "second"]]}}
            missing = BOOTSTRAP._required_missing(changed, target)

            self.assertIn("wse-dependency.json (identity mismatch)", missing)

    def test_partial_target_is_rejected_without_force(self):
        package = {
            "name": "sample",
            "version": "1.0.0",
            "install_subdir": "sample",
            "required_files": ["include/sample.h", "LICENSE"],
        }
        with tempfile.TemporaryDirectory() as temporary:
            vendor = Path(temporary) / "vendor"
            (vendor / "sample/include").mkdir(parents=True)
            (vendor / "sample/include/sample.h").write_text("partial", encoding="utf-8")
            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Dependency target is partial"):
                BOOTSTRAP.provision_package(package, Path(temporary) / "cache", vendor, offline=True)

    def test_git_cache_identity_uses_pinned_commit(self):
        commit = "a" * 40
        package = {
            "name": "sample-git",
            "version": "1.0.0",
            "source": {
                "type": "git",
                "checkout_dir": "sample-git-1.0.0",
                "commit": commit,
                "bundle": "sample-git-1.0.0.bundle",
            },
        }
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            BOOTSTRAP, "_git_head", return_value=commit
        ):
            cache = Path(temporary)
            checkout = cache / "sample-git-1.0.0"
            checkout.mkdir()
            resolved, identity = BOOTSTRAP._verify_git_cache(package, cache)
            self.assertEqual(resolved, checkout)
            self.assertIn(commit, identity)


    def _make_recipe_package(self, tag: str):
        code = (
            "import pathlib, sys; "
            "root = pathlib.Path(sys.argv[1]); "
            "(root / 'built.txt').write_text(sys.argv[2], encoding='utf-8'); "
            "counter = root / 'build-count.txt'; "
            "counter.write_text("
            "(counter.read_text(encoding='utf-8') if counter.exists() else '') + 'x'"
            ", encoding='utf-8')"
        )
        return {
            "name": "sample",
            "version": "1.0.0",
            "license": "MIT",
            "license_file": "built.txt",
            "install_subdir": "sample",
            "metadata_file": "dependency.json",
            "required_files": ["built.txt"],
            "archive_name": "sample.zip",
            "sha256": "",
            "config": {
                "extracted_dir": "sample-extract",
                "build_working_dir": "sample",
                "build": {
                    "commands": [
                        [sys.executable, "-c", code, "{extract_root}", tag],
                    ]
                },
                "copy_rules": [{"from": "built.txt", "to": "built.txt", "type": "file"}],
            },
        }

    def _write_sample_archive(self, cache: Path) -> str:
        cache.mkdir(parents=True, exist_ok=True)
        archive = cache / "sample.zip"
        with zipfile.ZipFile(archive, "w") as bundle:
            bundle.writestr("sample/source.txt", "source")
        return BOOTSTRAP._sha256(archive)

    def test_changed_build_command_rebuilds_the_cached_outputs(self):
        # A build recipe change must not be masked by build outputs that an
        # earlier recipe left in the extraction cache.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "cache"
            vendor = root / "vendor"
            digest = self._write_sample_archive(cache)

            first = self._make_recipe_package("v1")
            first["sha256"] = digest
            BOOTSTRAP.provision_package(
                first, cache, vendor, offline=True, cmake=sys.executable
            )
            self.assertEqual((vendor / "sample" / "built.txt").read_text(encoding="utf-8"), "v1")
            self.assertEqual(
                (cache / "sample-extract" / "build-count.txt").read_text(encoding="utf-8"), "x"
            )

            second = self._make_recipe_package("v2")
            second["sha256"] = digest
            # The installed target now carries a different recipe identity, so replacing it
            # still requires the explicit opt-in.
            with self.assertRaisesRegex(
                BOOTSTRAP.BootstrapError, "was produced by a different recipe"
            ):
                BOOTSTRAP.provision_package(
                    second, cache, vendor, offline=True, cmake=sys.executable
                )
            BOOTSTRAP.provision_package(
                second, cache, vendor, offline=True, force=True, cmake=sys.executable
            )
            self.assertEqual((vendor / "sample" / "built.txt").read_text(encoding="utf-8"), "v2")
            self.assertEqual(
                (cache / "sample-extract" / "build-count.txt").read_text(encoding="utf-8"), "xx"
            )

    def test_unchanged_build_command_reuses_the_cached_outputs(self):
        # Reinstalling from an unchanged recipe must not pay for a rebuild.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "cache"
            vendor = root / "vendor"
            digest = self._write_sample_archive(cache)

            package = self._make_recipe_package("v1")
            package["sha256"] = digest
            BOOTSTRAP.provision_package(
                package, cache, vendor, offline=True, cmake=sys.executable
            )
            shutil.rmtree(vendor / "sample")

            BOOTSTRAP.provision_package(
                package, cache, vendor, offline=True, cmake=sys.executable
            )
            self.assertEqual((vendor / "sample" / "built.txt").read_text(encoding="utf-8"), "v1")
            self.assertEqual(
                (cache / "sample-extract" / "build-count.txt").read_text(encoding="utf-8"), "x"
            )


class LanguageSelectionBootstrapTests(unittest.TestCase):
    """Contracts for `--language` / `--component` selection and verification-tool provisioning."""

    @staticmethod
    def _repository_manifest():
        return json.loads((WSE_ROOT / "bootstrap-manifest.json").read_text(encoding="utf-8"))

    @staticmethod
    def _fake_toolchain(**overrides):
        toolchain = {
            "name": "wse-fake-toolchain",
            "role": "verification-tool",
            "version": "1.0.0",
            "license": "MIT",
            "install_subdir": "wse-fake-toolchain",
            "required_files": ["marker.txt"],
            "install_hint": "Install the fake toolchain by hand.",
            "detect": {
                "environment_roots": ["WSE_FAKE_TOOLCHAIN_HOME"],
                "executable": "wse-fake-toolchain",
            },
            "platforms": {
                BOOTSTRAP._host_platform(): {
                    "architectures": {
                        BOOTSTRAP._host_architecture(): {
                            "url": "https://example.invalid/fake.zip",
                            "archive_name": "fake.zip",
                            "sha256": "0" * 64,
                            "extracted_root": "fake-1.0.0",
                        }
                    }
                }
            },
        }
        toolchain.update(overrides)
        return toolchain

    def test_selection_profiles_only_reference_declared_packages_and_toolchains(self):
        manifest = self._repository_manifest()
        package_names = {package["name"] for package in manifest["packages"]}
        toolchain_names = {toolchain["name"] for toolchain in manifest["toolchains"]}

        self.assertEqual(set(manifest["language_profiles"]), {"cpp", "cs", "java", "js", "python"})
        self.assertEqual(
            set(manifest["component_profiles"]),
            {"xpt", "gef", "iui", "oui", "tmr", "vpj", "vpj-video"},
        )
        # A toolchain is a verification tool, so its name must never collide with a shipped
        # dependency; the two live in different trees and only one of them is redistributed.
        self.assertFalse(package_names & toolchain_names)

        for name, profile in manifest["language_profiles"].items():
            self.assertTrue(profile["summary"], name)
            self.assertLessEqual(set(profile["packages"]), package_names, name)
            self.assertLessEqual(set(profile["toolchains"]), toolchain_names, name)
        for name, profile in manifest["component_profiles"].items():
            self.assertLessEqual(set(profile["packages"]), package_names, name)
            self.assertLessEqual(set(profile["implies"]), set(manifest["component_profiles"]), name)
            self.assertTrue(profile["cmake"], name)

    def test_toolchains_are_verification_tools_that_pin_their_source_identity(self):
        manifest = self._repository_manifest()
        digest_lengths = {"sha256": 64, "sha512": 128}
        for toolchain in manifest["toolchains"]:
            name = toolchain["name"]
            # The role is what keeps a toolchain out of vendor/, the install package, and the SBOM.
            self.assertEqual(toolchain["role"], "verification-tool", name)
            self.assertTrue(toolchain["version"], name)
            self.assertTrue(toolchain["license"], name)
            self.assertTrue(toolchain["summary"], name)
            self.assertTrue(toolchain["detect"]["executable"], name)
            self.assertTrue(toolchain["cmake"], name)
            # Any host without a pinned archive has to be told what to install instead.
            self.assertTrue(toolchain["install_hint"], name)

            for platform_name, entry in toolchain["platforms"].items():
                for architecture, source in entry["architectures"].items():
                    label = f"{name}/{platform_name}/{architecture}"
                    self.assertTrue(source["url"].startswith("https://"), label)
                    self.assertTrue(source["archive_name"], label)
                    self.assertTrue(source["required_files"], label)
                    pinned = {
                        algorithm: value
                        for algorithm, value in source.items()
                        if algorithm in digest_lengths
                    }
                    self.assertTrue(pinned, f"{label} pins no digest")
                    for algorithm, value in pinned.items():
                        self.assertRegex(
                            value, rf"^[0-9a-f]{{{digest_lengths[algorithm]}}}$", label
                        )

    def test_every_language_toolchain_covers_the_supported_host_matrix(self):
        manifest = self._repository_manifest()
        toolchains = {entry["name"]: entry for entry in manifest["toolchains"]}

        def hosts(name):
            return {
                (platform_name, architecture)
                for platform_name, entry in toolchains[name]["platforms"].items()
                for architecture in entry["architectures"]
            }

        supported = {
            ("windows", "x86_64"),
            ("windows", "arm64"),
            ("linux", "x86_64"),
            ("linux", "arm64"),
        }
        for name in ("nodejs", "temurin-jdk", "dotnet-sdk"):
            self.assertEqual(hosts(name), supported, name)

        # CPython publishes a redistributable Windows build with development headers; on Linux the
        # interpreter and its headers come from the system package manager, so it is detect-only
        # there and the install hint has to cover that case.
        self.assertEqual(hosts("cpython"), {("windows", "x86_64"), ("windows", "arm64")})
        self.assertIn("apt", toolchains["cpython"]["install_hint"])

    def test_executable_candidates_follow_the_host(self):
        # Under WSL the Windows directories are on PATH, so a `.exe` candidate on Linux would let a
        # Windows dotnet.exe drive a Linux build.
        candidates = BOOTSTRAP._executable_candidates("dotnet")
        if os.name == "nt":
            self.assertIn("dotnet.exe", candidates)
        else:
            self.assertEqual(candidates, ["dotnet"])

    def test_an_alternative_executable_name_is_detected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "bin").mkdir()
            # Ubuntu installs CPython as `python3` and provides no `python`.
            (root / "bin" / "python3").write_text("interpreter", encoding="utf-8")

            spec = {"executable": "python", "executable_alternatives": ["python3"]}
            self.assertEqual(BOOTSTRAP._executable_for_root(root, spec), root / "bin" / "python3")
            self.assertIsNone(BOOTSTRAP._executable_for_root(root, {"executable": "python"}))

    def test_the_manifest_declares_the_posix_names_of_each_tool(self):
        manifest = self._repository_manifest()
        detect = {entry["name"]: entry["detect"] for entry in manifest["toolchains"]}
        self.assertIn("python3", detect["cpython"]["executable_alternatives"])
        self.assertIn("nodejs", detect["nodejs"]["executable_alternatives"])

    def test_a_toolchain_is_selected_by_host_architecture_not_the_cross_target(self):
        # A verification tool runs on the machine that builds, so a cross build must not pick an
        # archive this host cannot execute.
        toolchain = self._fake_toolchain()
        architectures = toolchain["platforms"][BOOTSTRAP._host_platform()]["architectures"]
        host = BOOTSTRAP._host_architecture()
        other = "arm64" if host == "x86_64" else "x86_64"
        architectures[other] = dict(architectures[host], archive_name="wrong-host.zip")

        self.assertEqual(
            BOOTSTRAP._toolchain_host_source(toolchain)["archive_name"], "fake.zip"
        )
        self.assertNotIn(
            "target_architecture",
            BOOTSTRAP.provision_toolchain.__code__.co_varnames,
            "provision_toolchain must not accept a cross target",
        )

    def test_required_files_follow_the_host_source(self):
        toolchain = self._fake_toolchain(required_files=["fallback.txt"])
        self.assertEqual(BOOTSTRAP._toolchain_required_files(toolchain), ["fallback.txt"])

        architectures = toolchain["platforms"][BOOTSTRAP._host_platform()]["architectures"]
        architectures[BOOTSTRAP._host_architecture()]["required_files"] = ["bin/tool"]
        self.assertEqual(BOOTSTRAP._toolchain_required_files(toolchain), ["bin/tool"])

    def test_a_sha512_pinned_archive_is_accepted_and_a_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "cache"
            cache.mkdir()
            archive = cache / "fake.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("fake-1.0.0/marker.txt", "payload")

            def toolchain_with(**digests):
                toolchain = self._fake_toolchain()
                source = toolchain["platforms"][BOOTSTRAP._host_platform()]["architectures"][
                    BOOTSTRAP._host_architecture()
                ]
                source.pop("sha256")
                source.update(digests)
                return toolchain

            payload = archive.read_bytes()
            resolved, _, result = BOOTSTRAP.provision_toolchain(
                toolchain_with(sha512=hashlib.sha512(payload).hexdigest()),
                cache,
                offline=True,
                allow_fetch=True,
            )
            self.assertIn("fetched", result)
            self.assertEqual((resolved / "marker.txt").read_text(encoding="utf-8"), "payload")

            # A second cache, because a complete cached toolchain is reused without re-reading the
            # archive it came from.
            rejected = root / "rejected-cache"
            rejected.mkdir()
            shutil.copyfile(archive, rejected / archive.name)
            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "SHA-512"):
                BOOTSTRAP.provision_toolchain(
                    toolchain_with(sha512="f" * 128), rejected, offline=True, allow_fetch=True
                )

    def test_both_pinned_digests_must_match(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "cache"
            cache.mkdir()
            archive = cache / "fake.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("fake-1.0.0/marker.txt", "payload")

            toolchain = self._fake_toolchain()
            source = toolchain["platforms"][BOOTSTRAP._host_platform()]["architectures"][
                BOOTSTRAP._host_architecture()
            ]
            source["sha256"] = hashlib.sha256(archive.read_bytes()).hexdigest()
            source["sha512"] = "0" * 128

            # A correct SHA-256 must not excuse a wrong SHA-512.
            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "SHA-512"):
                BOOTSTRAP.provision_toolchain(toolchain, cache, offline=True, allow_fetch=True)

    def test_an_archive_that_pins_no_digest_is_rejected_before_any_download(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            BOOTSTRAP.urllib.request, "urlopen"
        ) as urlopen:
            root = Path(temporary)
            toolchain = self._fake_toolchain()
            toolchain["platforms"][BOOTSTRAP._host_platform()]["architectures"][
                BOOTSTRAP._host_architecture()
            ].pop("sha256")

            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "pins no archive digest"):
                BOOTSTRAP.provision_toolchain(
                    toolchain, root / "cache", offline=False, allow_fetch=True
                )
            urlopen.assert_not_called()

    def test_component_selection_expands_implied_components(self):
        manifest = self._repository_manifest()
        packages, toolchains, cmake = BOOTSTRAP._resolve_selection(manifest, [], ["vpj-video"], "windows/x86_64")

        # The implication chain mirrors the auto-enable rules in CMakeLists.txt.
        self.assertEqual(cmake["WSE_BUILD_VPJ_VIDEO"], "ON")
        self.assertEqual(cmake["WSE_BUILD_VPJ"], "ON")
        self.assertEqual(cmake["WSE_BUILD_XPT"], "ON")
        self.assertEqual(set(packages), {"libjpeg-turbo", "nlohmann-json", "libcurl"})
        self.assertEqual(toolchains, [])

    def test_language_selection_collects_toolchains_and_binding_options(self):
        manifest = self._repository_manifest()
        packages, toolchains, cmake = BOOTSTRAP._resolve_selection(manifest, ["js", "java"], [], "windows/x86_64")

        self.assertEqual(packages, ["node-api-headers"])
        self.assertEqual(toolchains, ["nodejs", "temurin-jdk"])
        self.assertEqual(cmake["WSE_BUILD_NODE_BINDING"], "ON")
        self.assertEqual(cmake["WSE_BUILD_JAVA_BINDING"], "ON")

    def test_a_component_adds_the_packages_its_target_needs(self):
        manifest = self._repository_manifest()

        packages, _, _ = BOOTSTRAP._resolve_selection(manifest, [], ["xpt"], "linux/arm64")
        # ARM64 libcurl links the pinned OpenSSL it was cross-built against.
        self.assertEqual(set(packages), {"libcurl", "openssl"})

        for target in ("linux/x86_64", "windows/x86_64", "windows/arm64"):
            packages, _, _ = BOOTSTRAP._resolve_selection(manifest, [], ["xpt"], target)
            self.assertEqual(set(packages), {"libcurl"}, target)

    def test_the_selection_target_follows_the_cross_target_architecture(self):
        host = BOOTSTRAP._host_platform()
        self.assertEqual(
            BOOTSTRAP._selection_target("arm64"), f"{host}/arm64"
        )
        self.assertEqual(
            BOOTSTRAP._selection_target(None),
            f"{host}/{BOOTSTRAP._host_architecture()}",
        )

    def test_the_base_preset_follows_the_target_architecture(self):
        official = json.loads((WSE_ROOT / "CMakePresets.json").read_text(encoding="utf-8"))
        names = {entry["name"] for entry in official["configurePresets"]}

        with mock.patch.object(BOOTSTRAP, "_host_platform", return_value="linux"):
            with mock.patch.object(BOOTSTRAP, "_host_architecture", return_value="x86_64"):
                native = BOOTSTRAP._default_base_preset(None)
                # A cross build has to inherit the preset carrying the cross toolchain file.
                cross = BOOTSTRAP._default_base_preset("arm64")
        self.assertEqual(native, "linux-gcc-shared-core")
        self.assertEqual(cross, "linux-arm64-gcc-shared-core")
        self.assertIn(native, names)
        self.assertIn(cross, names)

        cross_preset = next(
            entry for entry in official["configurePresets"] if entry["name"] == cross
        )
        self.assertIn("CMAKE_TOOLCHAIN_FILE", cross_preset["cacheVariables"])

    def test_a_dependency_root_cmake_cannot_infer_is_pinned(self):
        manifest = self._repository_manifest()
        vendor = Path("/opt/wse-vendor")
        libcurl = next(item for item in manifest["packages"] if item["name"] == "libcurl")
        openssl = next(item for item in manifest["packages"] if item["name"] == "openssl")

        with mock.patch.object(BOOTSTRAP, "_host_platform", return_value="linux"):
            # CMake derives the libcurl directory without an architecture suffix, and an empty
            # WSE_OPENSSL_ROOT means the target system OpenSSL, so both have to be pinned here.
            cross = BOOTSTRAP._package_cmake_values(libcurl, vendor, "arm64")
            self.assertEqual(cross, {"WSE_LIBCURL_ROOT": "/opt/wse-vendor/libcurl-arm64"})
            self.assertEqual(
                BOOTSTRAP._package_cmake_values(openssl, vendor, "arm64"),
                {"WSE_OPENSSL_ROOT": "/opt/wse-vendor/openssl-arm64"},
            )
            # The native target needs no pin, because the default already resolves.
            self.assertEqual(BOOTSTRAP._package_cmake_values(libcurl, vendor, "x86_64"), {})

    def test_a_cross_selection_writes_a_preset_that_can_configure(self):
        manifest = self._repository_manifest()
        target = "linux/arm64"
        packages, _, options = BOOTSTRAP._resolve_selection(manifest, [], ["xpt"], target)
        by_name = {item["name"]: item for item in manifest["packages"]}

        with mock.patch.object(BOOTSTRAP, "_host_platform", return_value="linux"):
            variables = dict(options)
            for name in packages:
                variables.update(
                    BOOTSTRAP._package_cmake_values(by_name[name], Path("/opt/v"), "arm64")
                )

        # Everything an ARM64 XPT configure needs, which is what the official cross preset sets.
        self.assertEqual(variables["WSE_BUILD_XPT"], "ON")
        self.assertEqual(variables["WSE_LIBCURL_ROOT"], "/opt/v/libcurl-arm64")
        self.assertEqual(variables["WSE_OPENSSL_ROOT"], "/opt/v/openssl-arm64")

    def test_unknown_selection_names_are_rejected(self):
        manifest = self._repository_manifest()
        with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Unknown language"):
            BOOTSTRAP._resolve_selection(manifest, ["typescript"], [], "windows/x86_64")
        with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Unknown component"):
            BOOTSTRAP._resolve_selection(manifest, [], ["holodeck"], "windows/x86_64")
        with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Unknown toolchain"):
            BOOTSTRAP._selected_toolchains(manifest, ["borland"])

    def test_offline_missing_toolchain_archive_never_opens_a_url(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            BOOTSTRAP.urllib.request, "urlopen"
        ) as urlopen:
            root = Path(temporary)
            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Offline cache is missing"):
                BOOTSTRAP.provision_toolchain(
                    self._fake_toolchain(), root / "cache", offline=True, allow_fetch=True
                )
            urlopen.assert_not_called()

    def test_an_installed_toolchain_is_preferred_over_a_fetch(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            BOOTSTRAP.urllib.request, "urlopen"
        ) as urlopen:
            root = Path(temporary)
            installed = root / "installed"
            installed.mkdir()
            # Detection only has to locate the program; the version probe fails harmlessly and this
            # toolchain declares no minimum, so the already-installed copy wins.
            (installed / "wse-fake-toolchain").write_text("not a real program", encoding="utf-8")

            with mock.patch.dict(
                os.environ, {"WSE_FAKE_TOOLCHAIN_HOME": str(installed)}, clear=False
            ):
                resolved, executable, result = BOOTSTRAP.provision_toolchain(
                    self._fake_toolchain(), root / "cache", offline=False, allow_fetch=True
                )

            self.assertEqual(resolved, installed)
            self.assertEqual(executable.parent, installed)
            self.assertTrue(result.startswith("detected"), result)
            urlopen.assert_not_called()
            self.assertFalse((root / "cache").exists(), "detection must not populate the cache")

    def test_a_cached_toolchain_is_reused_without_fetching(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            BOOTSTRAP.urllib.request, "urlopen"
        ) as urlopen:
            root = Path(temporary)
            cache = root / "cache"
            toolchain = self._fake_toolchain()
            cached = BOOTSTRAP._toolchain_root(toolchain, cache)
            cached.mkdir(parents=True)
            (cached / "marker.txt").write_text("cached", encoding="utf-8")

            resolved, _, result = BOOTSTRAP.provision_toolchain(
                toolchain, cache, offline=False, allow_fetch=True
            )

            self.assertEqual(resolved, cached)
            self.assertTrue(result.startswith("cached"), result)
            urlopen.assert_not_called()

    def test_no_fetch_toolchain_fails_with_the_install_hint(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            BOOTSTRAP.urllib.request, "urlopen"
        ) as urlopen:
            root = Path(temporary)
            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "forbids fetching"):
                BOOTSTRAP.provision_toolchain(
                    self._fake_toolchain(), root / "cache", offline=False, allow_fetch=False
                )
            urlopen.assert_not_called()

    def test_a_toolchain_without_a_pinned_archive_reports_how_to_install_it(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Install the fake toolchain"):
                BOOTSTRAP.provision_toolchain(
                    self._fake_toolchain(platforms={}),
                    root / "cache",
                    offline=False,
                    allow_fetch=True,
                )

    def test_a_fetched_toolchain_lands_in_the_cache_and_never_in_vendor(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "cache"
            cache.mkdir()
            archive = cache / "fake.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("fake-1.0.0/marker.txt", "payload")

            toolchain = self._fake_toolchain()
            source = toolchain["platforms"][BOOTSTRAP._host_platform()]["architectures"][
                BOOTSTRAP._host_architecture()
            ]
            source["sha256"] = hashlib.sha256(archive.read_bytes()).hexdigest()

            resolved, _, result = BOOTSTRAP.provision_toolchain(
                toolchain, cache, offline=True, allow_fetch=True
            )

            self.assertEqual(resolved, cache / BOOTSTRAP.TOOLCHAIN_SUBDIR / "wse-fake-toolchain")
            self.assertEqual((resolved / "marker.txt").read_text(encoding="utf-8"), "payload")
            self.assertIn("fetched", result)
            # vendor/ holds only what WSE links against and redistributes.
            self.assertFalse((root / "vendor").exists())
            self.assertFalse((cache / BOOTSTRAP.TOOLCHAIN_STAGING / toolchain["name"]).exists())

    def test_a_corrupt_toolchain_archive_is_rejected_before_extraction(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "cache"
            cache.mkdir()
            with zipfile.ZipFile(cache / "fake.zip", "w") as output:
                output.writestr("fake-1.0.0/marker.txt", "tampered")

            with self.assertRaisesRegex(BOOTSTRAP.BootstrapError, "Checksum mismatch"):
                BOOTSTRAP.provision_toolchain(
                    self._fake_toolchain(), cache, offline=True, allow_fetch=True
                )
            self.assertFalse((cache / BOOTSTRAP.TOOLCHAIN_SUBDIR).exists())

    def test_toolchain_cmake_values_resolve_the_installed_paths(self):
        toolchain = {
            "name": "sample",
            "cmake": {"WSE_SAMPLE_HOME": "{root}", "WSE_SAMPLE_EXECUTABLE": "{executable}"},
        }
        values = BOOTSTRAP._toolchain_cmake_values(
            toolchain, Path("/opt/sample"), Path("/opt/sample/bin/sample")
        )
        self.assertEqual(values["WSE_SAMPLE_HOME"], Path("/opt/sample").as_posix())
        self.assertEqual(values["WSE_SAMPLE_EXECUTABLE"], Path("/opt/sample/bin/sample").as_posix())

        # A toolchain resolved to a root whose program could not be located must omit the variable
        # rather than write an unsubstituted placeholder into the preset.
        partial = BOOTSTRAP._toolchain_cmake_values(toolchain, Path("/opt/sample"), None)
        self.assertNotIn("WSE_SAMPLE_EXECUTABLE", partial)

    def test_selection_writes_a_user_preset_and_skips_the_default_packages(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            presets = root / "CMakeUserPresets.json"
            with mock.patch.object(BOOTSTRAP, "provision_package") as provision:
                exit_code = BOOTSTRAP.main(
                    [
                        "--language",
                        "cpp",
                        "--component",
                        "iui",
                        "--cache-dir",
                        str(root / "cache"),
                        "--vendor-root",
                        str(root / "vendor"),
                        "--user-presets",
                        str(presets),
                        "--no-fetch-toolchain",
                        "--offline",
                    ]
                )

            self.assertEqual(exit_code, 0)
            # IUI needs no vendored dependency, so an explicit selection must provision nothing.
            provision.assert_not_called()

            document = json.loads(presets.read_text(encoding="utf-8"))
            self.assertEqual(document["version"], 6)
            preset = document["configurePresets"][0]
            self.assertEqual(preset["name"], "wse-local")
            self.assertEqual(preset["cacheVariables"]["WSE_BUILD_IUI"], "ON")

            # The generated preset must inherit a preset that actually exists.
            official = json.loads((WSE_ROOT / "CMakePresets.json").read_text(encoding="utf-8"))
            names = {entry["name"] for entry in official["configurePresets"]}
            self.assertIn(preset["inherits"], names)
            self.assertIn(BOOTSTRAP._default_base_preset(), names)

    def test_a_relocated_vendor_root_reaches_the_generated_preset(self):
        # CMake defaults the dependency root to `<source>/vendor`; a preset that dropped a
        # relocated `--vendor-root` would configure against dependencies that are not there.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            presets = root / "CMakeUserPresets.json"
            vendor = root / "elsewhere"
            exit_code = BOOTSTRAP.main(
                [
                    "--component", "iui",
                    "--cache-dir", str(root / "cache"),
                    "--vendor-root", str(vendor),
                    "--user-presets", str(presets),
                    "--no-fetch-toolchain", "--offline",
                ]
            )
            self.assertEqual(exit_code, 0)
            variables = json.loads(presets.read_text(encoding="utf-8"))["configurePresets"][0][
                "cacheVariables"
            ]
            self.assertEqual(variables["WSE_DEPENDENCY_ROOT"], vendor.resolve().as_posix())

    def test_the_default_vendor_root_is_left_out_of_the_generated_preset(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            presets = root / "CMakeUserPresets.json"
            exit_code = BOOTSTRAP.main(
                [
                    "--component", "iui",
                    "--cache-dir", str(root / "cache"),
                    "--user-presets", str(presets),
                    "--no-fetch-toolchain", "--offline",
                ]
            )
            self.assertEqual(exit_code, 0)
            variables = json.loads(presets.read_text(encoding="utf-8"))["configurePresets"][0][
                "cacheVariables"
            ]
            self.assertNotIn("WSE_DEPENDENCY_ROOT", variables)

    def test_the_generated_preset_is_never_committed(self):
        ignored = (WSE_ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
        self.assertIn("/CMakeUserPresets.json", ignored)


if __name__ == "__main__":
    unittest.main()
