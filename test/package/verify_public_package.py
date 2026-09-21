"""Verify an installed WSE package at the public distribution boundary."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import zipfile
from pathlib import Path


PUBLIC_COMPONENTS = {"xpt", "gef", "iui", "oui", "tmr"}
TEXT_SUFFIXES = {
    ".cmake", ".h", ".hpp", ".js", ".json", ".md", ".py", ".pyi", ".txt"
}
# "vpj" is a public component name (owner decision, 2026-09-08): the WSE::Vpj target, the
# WSE_BUILD_VPJ option, and the binding availability keys already spell it in the public tree.
# What stays forbidden here is device and protocol identity.
PRIVATE_TEXT = re.compile(
    r"private-test-marker-1|private-test-marker-2|private-test-marker-3|private-test-marker-4|esc/?vp|private-test-marker-7|private-test-marker-8",
    re.IGNORECASE,
)
OS_CAMERA_TEXT = re.compile(
    r"windows\.h|dshow\.h|mfapi\.h|mfidl\.h|videodev2\.h|libcamera/|"
    r"\bIAMCameraControl\b|\bIAMVideoProcAmp\b|\bIMF[A-Za-z0-9_]*\b|"
    r"\bHRESULT\b|\bGUID\b|\bHANDLE\b|\bv4l2_[A-Za-z0-9_]*\b"
)


class VerificationError(RuntimeError):
    pass


def inspection_patterns() -> tuple[list[re.Pattern[str]], list[re.Pattern[str]]]:
    """Keep concrete private identifiers external; missing/invalid configured policies fail."""
    text_patterns = [PRIVATE_TEXT]
    path_patterns = [PRIVATE_TEXT]
    configured = os.environ.get("WSE_EXPORT_DENYLIST")
    if configured:
        policy = json.loads(Path(configured).read_text(encoding="utf-8"))
        for key, output in (("blocked_path_regexes", path_patterns),
                            ("blocked_text_regexes", text_patterns)):
            values = policy.get(key)
            if not isinstance(values, list) or not values or any(
                    not isinstance(value, str) or not value for value in values):
                raise VerificationError("The configured external inspection policy is incomplete.")
            try:
                output.extend(re.compile(value, re.IGNORECASE) for value in values)
            except re.error as error:
                raise VerificationError("The external inspection policy contains an invalid expression.") from error
    return path_patterns, text_patterns


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def verify_package(
    package_root: Path,
    expected_components: set[str],
    expected_bindings: set[str],
) -> None:
    package_root = package_root.resolve()
    manifest_path = package_root / "wse-package.json"
    if not manifest_path.is_file():
        raise VerificationError(f"Missing package manifest: {manifest_path}")
    manifest = json.loads(_read_text(manifest_path))
    components = manifest.get("components", {})
    path_patterns, text_patterns = inspection_patterns()
    if manifest.get("extension_commit") not in (None, "", "none") or components.get("vpj", False):
        raise VerificationError("A public package must not include an extension build.")
    if manifest.get("license") != "Apache-2.0" or manifest.get("license_files") != ["LICENSE", "NOTICE"]:
        raise VerificationError("A public package must identify its Apache-2.0 license and notices.")
    for name in ("LICENSE", "NOTICE"):
        path = package_root / name
        if not path.is_file() or path.read_bytes() != (Path(__file__).resolve().parents[2] / name).read_bytes():
            raise VerificationError("Missing or altered public license/notice file: " + name)

    unknown = expected_components - PUBLIC_COMPONENTS
    if unknown:
        raise VerificationError(f"Unknown public component expectation: {sorted(unknown)}")
    for component in sorted(PUBLIC_COMPONENTS):
        actual = bool(components.get(component, False))
        expected = component in expected_components
        if actual != expected:
            raise VerificationError(
                f"Component {component!r}: expected {expected}, package reports {actual}"
            )
    for binding in ("node", "python", "java", "dotnet"):
        actual = bool(components.get(binding, False))
        expected = binding in expected_bindings
        if actual != expected:
            raise VerificationError(
                f"Binding {binding!r}: expected {expected}, package reports {actual}"
            )

    paths = [path for path in package_root.rglob("*") if path.is_file()]
    for path in paths:
        relative = path.relative_to(package_root).as_posix()
        lowered = relative.lower()
        if "/device/legacy/" in f"/{lowered}" or lowered.endswith("/device/legacy"):
            raise VerificationError(f"Legacy WebCamera path leaked into package: {relative}")
        if any(pattern.search(relative) for pattern in path_patterns):
            raise VerificationError(f"Private path leaked into package: {relative}")
        if path.suffix.lower() in TEXT_SUFFIXES:
            content = _read_text(path)
            if any(pattern.search(content) for pattern in text_patterns):
                raise VerificationError(f"Private text leaked into package: {relative}")

    if "tmr" in expected_components:
        webcamera = package_root / "include/wse/api/tmr/device/WebCamera.h"
        if not webcamera.is_file():
            raise VerificationError(f"Missing canonical WebCamera header: {webcamera}")
        tmr_root = package_root / "include/wse/api/tmr"
        for header in tmr_root.rglob("*.h"):
            if OS_CAMERA_TEXT.search(_read_text(header)):
                raise VerificationError(
                    f"Operating-system camera API leaked into public header: {header}"
                )

    language_files: list[Path] = []
    if "node" in expected_bindings:
        language_files.extend([
            package_root / "lang/js/index.js",
            package_root / "lang/js/index.d.ts",
        ])
    if "python" in expected_bindings:
        language_files.extend([
            package_root / "lang/python/wse/__init__.py",
            package_root / "lang/python/wse/__init__.pyi",
        ])
    for path in language_files:
        if not path.is_file():
            raise VerificationError(f"Missing binding package file: {path}")
        content = _read_text(path)
        if "CameraSession" in content or "CameraOpenDescription" in content:
            raise VerificationError(f"Old camera binding leaked into package: {path}")
        if "WebCamera" not in content:
            raise VerificationError(f"WebCamera is missing from binding package: {path}")

    if "java" in expected_bindings:
        jar_path = package_root / "lang/java/wse.jar"
        if not jar_path.is_file():
            raise VerificationError(f"Missing Java binding JAR: {jar_path}")
        with zipfile.ZipFile(jar_path) as archive:
            entries = set(archive.namelist())
        required = "io/wapitistew/wse/WebCamera.class"
        if required not in entries:
            raise VerificationError(f"WebCamera class is missing from {jar_path}")
        for old_name in (
            "io/wapitistew/wse/CameraSession.class",
            "io/wapitistew/wse/CameraOpenDescription.class",
        ):
            if old_name in entries:
                raise VerificationError(f"Old camera binding leaked into {jar_path}: {old_name}")

    if "dotnet" in expected_bindings:
        assembly = package_root / "lang/cs/WapitiStew.Wse.dll"
        if not assembly.is_file():
            raise VerificationError(f"Missing .NET binding assembly: {assembly}")
        native = [
            package_root / "bin/wse_capi.dll",
            package_root / "bin/libwse_capi.so",
            package_root / "lib/libwse_capi.so",
        ]
        if not any(path.is_file() for path in native):
            raise VerificationError(
                "Missing the flat C ABI native library required by the .NET binding"
            )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-root", type=Path, required=True)
    parser.add_argument("--expect-component", action="append", default=[])
    parser.add_argument(
        "--expect-binding", choices=("node", "python", "java", "dotnet"), action="append", default=[]
    )
    arguments = parser.parse_args()
    try:
        verify_package(
            arguments.package_root,
            {value.lower() for value in arguments.expect_component},
            set(arguments.expect_binding),
        )
    except (OSError, ValueError, VerificationError, zipfile.BadZipFile) as error:
        print(f"public package verification failed: {error}", file=sys.stderr)
        return 1
    print(f"public package verification passed: {arguments.package_root.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
