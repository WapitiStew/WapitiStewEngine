#!/usr/bin/env python3

import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock


WSE_ROOT = Path(__file__).resolve().parents[2]


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(module)
    return module


CREATE = load_module("wse_create_offline_kit", WSE_ROOT / "tools/offline/create_offline_kit.py")
VERIFY = load_module("wse_verify_offline_kit", WSE_ROOT / "tools/offline/verify_offline_kit.py")


class OfflineKitTests(unittest.TestCase):
    def make_package(self, root: Path, private_text: str = "") -> Path:
        package = root / "package"
        (package / "cmake").mkdir(parents=True)
        (package / "include/wse/api/xpt").mkdir(parents=True)
        (package / "vendor/libcurl").mkdir(parents=True)
        (package / "wse-package.json").write_text(
            json.dumps(
                {
                    "name": "wonderstew-engine",
                    "version": "0.1.0",
                    "library_type": "SHARED",
                    "source_commit": "2" * 40,
                    "components": {
                        "xpt": True,
                        "gef": False,
                        "iui": False,
                        "oui": False,
                        "tmr": False,
                    },
                }
            ),
            encoding="utf-8",
        )
        (package / "cmake/WonderStewEngineConfig.cmake").write_text(
            "add_library(WSE::Core IMPORTED)\n" + private_text,
            encoding="utf-8",
        )
        (package / "include/wse/api/xpt/stew.h").write_text("#pragma once\n", encoding="utf-8")
        (package / "vendor/libcurl/wse-dependency.json").write_text(
            json.dumps(
                {
                    "schema_version": 2,
                    "name": "libcurl",
                    "version": "8.21.0",
                    "license": "curl",
                    "recipe_sha256": "a" * 64,
                }
            ),
            encoding="utf-8",
        )
        return package

    def make_consumer(self, root: Path) -> Path:
        consumer = root / "consumer"
        consumer.mkdir()
        (consumer / "CMakeLists.txt").write_text("cmake_minimum_required(VERSION 3.24)\n", encoding="utf-8")
        (consumer / "main.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
        return consumer

    OVERLAY_IDENTITY = {"commit": "4" * 40, "tree": "3" * 40}

    def make_controlled_package(self, root: Path, residue: str = "") -> Path:
        package = self.make_package(root)
        metadata = json.loads((package / "wse-package.json").read_text(encoding="utf-8"))
        metadata["components"]["vpj"] = True
        metadata["extension_commit"] = self.OVERLAY_IDENTITY["tree"]
        (package / "wse-package.json").write_text(json.dumps(metadata), encoding="utf-8")
        (package / "include/wse/api/vpj").mkdir(parents=True)
        (package / "include/wse/api/vpj/stew.h").write_text(
            "#pragma once\n// vpj overlay header\n", encoding="utf-8"
        )
        if residue:
            (package / residue).write_text("residue", encoding="utf-8")
        return package

    def create_controlled_candidate(self, package: Path, consumer: Path, output: Path, **overrides):
        kit_id = overrides.pop("kit_id", "wse-confidential-0.1.0-windows-x86_64-shared-vpj")
        arguments = {
            "classification": "controlled",
            "target_os": "windows",
            "target_arch": "x86_64",
            "overlay_identity": dict(self.OVERLAY_IDENTITY),
        }
        arguments.update(overrides)
        return CREATE.create_candidate(
            package,
            output,
            kit_id,
            "MSVC",
            "14.43",
            {"commit": "1" * 40, "tree": "2" * 40},
            consumer,
            **arguments,
        )

    def test_candidate_refuses_a_mismatched_engine_tree(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(CREATE.KitError, "Engine source tree"):
                CREATE.create_candidate(self.make_package(root), root / "kit", "mismatch",
                                        "MSVC", "14.43", {"commit": "1" * 40, "tree": "9" * 40},
                                        self.make_consumer(root))
            self.assertFalse((root / "kit").exists())

    def test_v2_refuses_old_formats_extra_metadata_and_remote_provider(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            kit = root / "kit"
            path = CREATE.create_candidate(self.make_package(root), kit, "contract", "MSVC", "14.43",
                                           {"commit": "1" * 40, "tree": "2" * 40}, self.make_consumer(root))
            original = json.loads(path.read_text(encoding="utf-8"))
            for mutation, diagnostic in (
                ({"format": "wse-offline-kit-candidate-v1"}, "Unsupported candidate format"),
                ({"application": {"commit": "3" * 40}}, "Unexpected candidate fields"),
                ({"provider": {"default": "source"}}, "offline package provider"),
            ):
                with self.subTest(mutation=mutation):
                    path.write_text(json.dumps(dict(original, **mutation)), encoding="utf-8")
                    with mock.patch.object(VERIFY, "_run") as run:
                        with self.assertRaisesRegex(VERIFY.VerificationError, diagnostic):
                            VERIFY.accept_kit(kit, root / "accepted.json", "cmake", "Debug", True,
                                              network_probe=lambda: (True, []))
                        run.assert_not_called()
                    self.assertFalse((root / "accepted.json").exists())

    def test_deterministic_zip_has_stable_checksum(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = self.make_consumer(root)
            first = root / "first.zip"
            second = root / "second.zip"

            CREATE.deterministic_zip(source, first)
            CREATE.deterministic_zip(source, second)

            self.assertEqual(hashlib.sha256(first.read_bytes()).hexdigest(), hashlib.sha256(second.read_bytes()).hexdigest())

    def test_candidate_contains_verified_artifacts_and_dependency_identity(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_package(root)
            consumer = self.make_consumer(root)
            output = root / "kit"
            identity = {"commit": "1" * 40, "tree": "2" * 40}

            candidate_path = CREATE.create_candidate(
                package,
                output,
                "wse-0.1.0-windows-x86_64-shared-xpt",
                "MSVC",
                "14.43",
                identity,
                consumer,
            )
            candidate = json.loads(candidate_path.read_text(encoding="utf-8"))

            self.assertEqual(candidate["format"], "wse-offline-kit-candidate-v2")
            self.assertEqual(candidate["wse"]["repository"], "Engine")
            self.assertEqual(candidate["wse"]["commit"], identity["commit"])
            self.assertEqual(set(candidate), {"format", "kitId", "classification", "target",
                                            "wse", "provider", "artifacts", "dependencies", "acceptance"})
            self.assertEqual((output / "OfflineKitManifest.schema.json").read_bytes(),
                             (WSE_ROOT / "tools/offline/OfflineKitManifest.schema.json").read_bytes())
            self.assertEqual((output / candidate["acceptance"]["verifier"]).read_bytes(),
                             (WSE_ROOT / "tools/offline/verify_offline_kit.py").read_bytes())
            self.assertEqual(candidate["acceptance"]["status"], "NOT_RUN")
            self.assertEqual(candidate["dependencies"][0]["recipe_sha256"], "a" * 64)
            VERIFY.verify_checksums(output, candidate)

    def test_public_candidate_rejects_private_token(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_package(root, CREATE._compiled_denylist()[1][0].decode("ascii"))
            consumer = self.make_consumer(root)
            with self.assertRaisesRegex(CREATE.KitError, "blocked private token"):
                CREATE.create_candidate(
                    package,
                    root / "kit",
                    "blocked",
                    "MSVC",
                    "14.43",
                    {"commit": "1" * 40, "tree": "2" * 40},
                    consumer,
                )

    def test_built_in_denylist_blocks_controlled_paths_without_the_private_list(self):
        # The full list lives with the controlled material, outside this repository. The gate has
        # to refuse the paths it knows about whether or not that list is reachable.
        self.assertIsNone(CREATE.DENYLIST)
        patterns, tokens = CREATE._compiled_denylist()
        blocked = [
            "api/vpj/stew.h",
            "core/tmr/device/private-test-marker-1.cpp",
            "API/VPJ/stew.h",
            "doc/report/phase0/notes.md",
            "lang/js/native/vpj_addon.cpp",
        ]
        for path in blocked:
            self.assertTrue(
                any(pattern.search(path) for pattern in patterns),
                f"built-in denylist must block {path}",
            )
        self.assertFalse(any(pattern.search("api/tmr/device/WebCamera.h") for pattern in patterns))
        self.assertNotIn(b"vpj", tokens)

    def test_public_component_capabilities_are_allowed_but_build_residue_is_not(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_package(root, "# WSE_BUILD_VPJ is an optional component flag\n")
            header = package / "include/wse/api/xpt/stew.h"
            header.write_text("#pragma once\nstruct Capabilities { bool has_vpj; };\n", encoding="utf-8")
            CREATE.scan_public_tree(package)
            (package / "debug.pdb").write_bytes(b"diagnostic symbols")
            with self.assertRaisesRegex(CREATE.KitError, "path is blocked"):
                CREATE.scan_public_tree(package)

    def test_overlay_denylist_adds_to_the_built_in_list_and_never_replaces_it(self):
        with tempfile.TemporaryDirectory() as temporary:
            overlay = Path(temporary) / "denylist.json"
            overlay.write_text(
                json.dumps({"blocked_path_regexes": [r"(^|/)secret-thing(/|$)"]}),
                encoding="utf-8",
            )
            original = CREATE.DENYLIST
            try:
                CREATE.DENYLIST = overlay
                patterns, _ = CREATE._compiled_denylist()
                self.assertTrue(any(p.search("a/secret-thing/b") for p in patterns))
                # The built-in entries survive; an overlay cannot narrow the gate.
                self.assertTrue(any(p.search("api/vpj/stew.h") for p in patterns))
            finally:
                CREATE.DENYLIST = original

    def test_unusable_overlay_denylist_is_refused_rather_than_ignored(self):
        with tempfile.TemporaryDirectory() as temporary:
            original = CREATE.DENYLIST
            try:
                CREATE.DENYLIST = Path(temporary) / "absent.json"
                with self.assertRaisesRegex(CREATE.KitError, "does not name a file"):
                    CREATE._compiled_denylist()
                empty = Path(temporary) / "empty.json"
                empty.write_text(json.dumps({}), encoding="utf-8")
                CREATE.DENYLIST = empty
                with self.assertRaisesRegex(CREATE.KitError, "no blocked_path_regexes"):
                    CREATE._compiled_denylist()
            finally:
                CREATE.DENYLIST = original

    def test_external_policy_rejects_text_and_utf16_binary_content(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            policy = root / "policy.json"
            policy.write_text(json.dumps({
                "blocked_path_regexes": [r"(^|/)restricted(/|$)"],
                "blocked_text_regexes": [r"fictional-device-[0-9]+"],
                "blocked_text_tokens": ["fictional-secret"],
            }), encoding="utf-8")
            original = CREATE.DENYLIST
            try:
                CREATE.DENYLIST = policy
                package = root / "package"
                package.mkdir()
                payload = package / "payload.bin"
                for content in (b"fictional-secret", b"fictional-device-12",
                                "fictional-device-12".encode("utf-16-be")):
                    payload.write_bytes(content)
                    with self.assertRaises(CREATE.KitError):
                        CREATE.scan_public_tree(package)
                payload.write_bytes(b"ordinary public payload")
                CREATE.scan_public_tree(package)
            finally:
                CREATE.DENYLIST = original
    def test_binary_scan_ignores_isolated_random_token_but_rejects_symbol_like_string(self):
        self.assertFalse(CREATE._contains_blocked_content(b"\x00VPJ\x00", [b"vpj"]))
        self.assertTrue(CREATE._contains_blocked_content(b"\x00WSE_VPJ_EXPORT\x00", [b"vpj"]))

    def test_controlled_candidate_records_private_overlay_and_classification(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_controlled_package(root)
            consumer = self.make_consumer(root)
            output = root / "kit"

            candidate_path = self.create_controlled_candidate(package, consumer, output)
            candidate = json.loads(candidate_path.read_text(encoding="utf-8"))

            self.assertEqual(candidate["classification"], "private-internal")
            self.assertEqual(candidate["privateOverlay"]["repository"], "private-overlay")
            self.assertEqual(candidate["privateOverlay"]["commit"], self.OVERLAY_IDENTITY["commit"])
            self.assertEqual(candidate["privateOverlay"]["requiresWse"], "0.1.0")
            self.assertIn("vpj", candidate["wse"]["modules"])
            for artifact in candidate["artifacts"]:
                self.assertEqual(artifact["classification"], "private-internal")
            VERIFY.verify_checksums(output, candidate)

    def test_controlled_candidate_requires_the_vpj_component(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_package(root)
            consumer = self.make_consumer(root)
            with self.assertRaisesRegex(CREATE.KitError, "Core/XPT/VPJ"):
                self.create_controlled_candidate(package, consumer, root / "kit")

    def test_public_candidate_refuses_a_controlled_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_controlled_package(root)
            consumer = self.make_consumer(root)
            with self.assertRaisesRegex(CREATE.KitError, "Core/XPT-only"):
                CREATE.create_candidate(
                    package,
                    root / "kit",
                    "wse-0.1.0-windows-x86_64-shared-xpt",
                    "MSVC",
                    "14.43",
                    {"commit": "1" * 40, "tree": "2" * 40},
                    consumer,
                )

    def test_controlled_kit_id_requires_the_confidential_prefix(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_controlled_package(root)
            consumer = self.make_consumer(root)
            with self.assertRaisesRegex(CREATE.KitError, "must start with"):
                self.create_controlled_candidate(
                    package, consumer, root / "kit", kit_id="wse-0.1.0-shared-vpj"
                )

    def test_controlled_candidate_refuses_build_residue(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_controlled_package(root, residue="core.pdb")
            consumer = self.make_consumer(root)
            with self.assertRaisesRegex(CREATE.KitError, "build residue"):
                self.create_controlled_candidate(package, consumer, root / "kit")

    def test_controlled_candidate_refuses_a_mismatched_overlay_tree(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_controlled_package(root)
            consumer = self.make_consumer(root)
            mismatched = dict(self.OVERLAY_IDENTITY, tree="9" * 40)
            with self.assertRaisesRegex(CREATE.KitError, "does not match"):
                self.create_controlled_candidate(
                    package, consumer, root / "kit", overlay_identity=mismatched
                )

    def test_linux_network_state_certifies_only_downed_interfaces(self):
        with tempfile.TemporaryDirectory() as temporary:
            sys_class_net = Path(temporary)
            (sys_class_net / "lo").mkdir()
            (sys_class_net / "lo/operstate").write_text("unknown\n", encoding="ascii")
            (sys_class_net / "eth0").mkdir()
            (sys_class_net / "eth0/operstate").write_text("down\n", encoding="ascii")

            disabled, enabled = VERIFY.linux_network_state(sys_class_net)
            self.assertTrue(disabled)
            self.assertEqual(enabled, [])

            (sys_class_net / "wlan0").mkdir()
            (sys_class_net / "wlan0/operstate").write_text("up\n", encoding="ascii")
            disabled, enabled = VERIFY.linux_network_state(sys_class_net)
            self.assertFalse(disabled)
            self.assertEqual(enabled, ["wlan0 (up)"])

    def test_acceptance_refuses_enabled_network(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(VERIFY.VerificationError, "Every network adapter"):
                VERIFY.accept_kit(
                    Path(temporary),
                    Path(temporary) / "manifest.json",
                    "cmake",
                    "Debug",
                    True,
                    network_probe=lambda: (False, ["Ethernet"]),
                )

    def test_acceptance_manifest_records_network_and_consumer_gate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_package(root)
            consumer = self.make_consumer(root)
            kit = root / "kit"
            CREATE.create_candidate(
                package,
                kit,
                "accepted-test",
                "MSVC",
                "14.43",
                {"commit": "1" * 40, "tree": "2" * 40},
                consumer,
                target_os=VERIFY.host_os_name(),
            )
            output = root / "accepted.json"

            with mock.patch.object(VERIFY, "_run", return_value="ok"):
                VERIFY.accept_kit(
                    kit,
                    output,
                    "cmake",
                    "Debug",
                    True,
                    network_probe=lambda: (True, []),
                )

            manifest = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(manifest["schemaVersion"], 2)
            self.assertEqual(manifest["wse"]["repository"], "Engine")
            self.assertEqual(set(manifest), {"schemaVersion", "kitId", "classification", "createdAt",
                                           "target", "wse", "provider", "artifacts", "dependencies", "verification"})
            self.assertTrue(manifest["verification"]["networkDisabled"])
            self.assertEqual(manifest["verification"]["remoteAttemptCount"], 0)
            self.assertEqual(manifest["verification"]["run"], "PASS")

    def test_acceptance_refuses_a_target_host_mismatch(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_package(root)
            consumer = self.make_consumer(root)
            kit = root / "kit"
            mismatched = "linux" if VERIFY.host_os_name() == "windows" else "windows"
            CREATE.create_candidate(
                package,
                kit,
                "mismatch-test",
                "MSVC",
                "14.43",
                {"commit": "1" * 40, "tree": "2" * 40},
                consumer,
                target_os=mismatched,
            )
            with self.assertRaisesRegex(VERIFY.VerificationError, "verifying host"):
                VERIFY.accept_kit(
                    kit,
                    root / "accepted.json",
                    "cmake",
                    "Debug",
                    True,
                    network_probe=lambda: (True, []),
                )

    def test_acceptance_carries_the_private_overlay_into_the_accepted_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = self.make_controlled_package(root)
            consumer = self.make_consumer(root)
            kit = root / "kit"
            self.create_controlled_candidate(
                package, consumer, kit, target_os=VERIFY.host_os_name()
            )
            output = root / "accepted.json"

            with mock.patch.object(VERIFY, "_run", return_value="ok"):
                VERIFY.accept_kit(
                    kit,
                    output,
                    "cmake",
                    "Debug",
                    True,
                    network_probe=lambda: (True, []),
                )

            manifest = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(manifest["classification"], "private-internal")
            self.assertEqual(manifest["privateOverlay"]["commit"], self.OVERLAY_IDENTITY["commit"])
            self.assertEqual(manifest["privateOverlay"]["requiresWse"], "0.1.0")


if __name__ == "__main__":
    unittest.main()
