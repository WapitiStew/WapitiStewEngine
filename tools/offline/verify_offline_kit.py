#!/usr/bin/env python3
"""Accept a WSE offline kit on a clean, network-disabled Windows or Linux machine."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple


class VerificationError(RuntimeError):
    """An offline-kit acceptance failure."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _read_json(path: Path) -> Dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise VerificationError(f"Cannot read JSON file {path}: {exc}") from exc


def host_os_name() -> str:
    if os.name == "nt":
        return "windows"
    if sys.platform.startswith("linux"):
        return "linux"
    return sys.platform


def windows_network_state() -> Tuple[bool, List[str]]:
    if os.name != "nt":
        return False, ["Network state can only be certified by this verifier on Windows."]
    command = [
        "powershell.exe",
        "-NoProfile",
        "-NonInteractive",
        "-Command",
        "Get-NetAdapter | Select-Object Name,Status | ConvertTo-Json -Compress",
    ]
    completed = subprocess.run(command, check=False, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if completed.returncode != 0:
        raise VerificationError(completed.stderr.strip() or "Get-NetAdapter failed.")
    if not completed.stdout.strip():
        return True, []
    data = json.loads(completed.stdout)
    adapters = data if isinstance(data, list) else [data]
    enabled = [str(item.get("Name", "unknown")) for item in adapters if item.get("Status") == "Up"]
    return not enabled, enabled


def linux_network_state(sys_class_net: Path = Path("/sys/class/net")) -> Tuple[bool, List[str]]:
    """Certify from sysfs alone, because a clean offline machine need not carry iproute2."""
    if not sys_class_net.is_dir():
        return False, ["Network state can only be certified when /sys/class/net is readable."]
    enabled: List[str] = []
    for interface in sorted(sys_class_net.iterdir()):
        # The loopback device stays up on any healthy machine and reaches nothing external.
        if interface.name == "lo":
            continue
        try:
            operstate = (interface / "operstate").read_text(encoding="ascii").strip()
        except OSError:
            operstate = "unreadable"
        # Fail closed: only an administratively downed interface counts as disabled. "unknown"
        # or an unreadable state is treated as enabled rather than assumed safe.
        if operstate != "down":
            enabled.append(f"{interface.name} ({operstate})")
    return not enabled, enabled


def host_network_state() -> Tuple[bool, List[str]]:
    if os.name == "nt":
        return windows_network_state()
    if sys.platform.startswith("linux"):
        return linux_network_state()
    return False, [f"Network state cannot be certified on this platform: {sys.platform}"]


def _safe_extract(archive_path: Path, destination: Path) -> None:
    with zipfile.ZipFile(archive_path) as archive:
        for member in archive.infolist():
            resolved = (destination / member.filename).resolve()
            try:
                resolved.relative_to(destination.resolve())
            except ValueError as exc:
                raise VerificationError(f"Unsafe archive member: {member.filename}") from exc
        archive.extractall(destination)


def _run(command: Sequence[str], environment: Dict[str, str]) -> str:
    completed = subprocess.run(
        list(command),
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=environment,
    )
    if completed.returncode != 0:
        raise VerificationError(
            f"Command failed ({completed.returncode}): {' '.join(command)}\n{completed.stdout}"
        )
    return completed.stdout


def _scan_config_for_remote_access(package_root: Path) -> None:
    blocked = (b"fetchcontent", b"externalproject_add", b"git clone", b"http://", b"https://")
    candidates = [package_root / "wse-package.json", *sorted((package_root / "cmake").glob("*"))]
    for path in candidates:
        if path.is_file():
            content = path.read_bytes().lower()
            if any(token in content for token in blocked):
                raise VerificationError(f"Installed package configuration contains a remote access directive: {path}")


def verify_checksums(kit_root: Path, candidate: Dict) -> None:
    for artifact in candidate.get("artifacts", []):
        path = kit_root / artifact["path"]
        if not path.is_file():
            raise VerificationError(f"Kit artifact is missing: {path}")
        if path.stat().st_size != artifact["size"] or sha256(path) != artifact["sha256"]:
            raise VerificationError(f"Kit artifact checksum mismatch: {path}")


def _schema_dependency(dependency: Dict) -> Dict:
    return {
        "name": dependency["name"],
        "version": dependency["version"],
        "license": dependency["license"],
        "artifact": dependency["artifact"],
    }


def _check_private_overlay(overlay) -> None:
    if not isinstance(overlay, dict):
        raise VerificationError("A private-internal candidate must carry its private overlay identity.")
    for key in ("repository", "version", "requiresWse"):
        if not str(overlay.get(key, "")).strip():
            raise VerificationError(f"Private overlay identity is missing {key}.")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", str(overlay.get("commit", ""))):
        raise VerificationError("Private overlay commit must be a 40-hex identity.")


def accept_kit(
    kit_root: Path,
    output_manifest: Path,
    cmake: str,
    configuration: str,
    confirm_clean_host: bool,
    network_probe=host_network_state,
) -> Path:
    if not confirm_clean_host:
        raise VerificationError("Pass --confirm-clean-host only on the intended clean validation PC.")
    network_disabled, enabled_adapters = network_probe()
    if not network_disabled:
        details = ", ".join(enabled_adapters) if enabled_adapters else "unknown state"
        raise VerificationError(f"Every network adapter must be disabled; enabled or uncertified: {details}")

    candidate_path = kit_root / "offline-kit-candidate.json"
    candidate = _read_json(candidate_path)
    if candidate.get("format") != "wse-offline-kit-candidate-v2":
        raise VerificationError(f"Unsupported candidate format: {candidate_path}")
    expected_fields = {"format", "kitId", "classification", "target", "wse", "provider",
                       "artifacts", "dependencies", "acceptance", "privateOverlay"}
    if set(candidate) - expected_fields:
        raise VerificationError("Unexpected candidate fields; application metadata belongs to its owner.")
    if candidate.get("provider") != {
        "default": "package", "allowed": ["package"], "remoteAccessAllowed": False
    }:
        raise VerificationError("Only the offline package provider is allowed.")
    identity = candidate.get("wse", {})
    if identity.get("repository") != "Engine" or any(
        not re.fullmatch(r"[0-9a-fA-F]{40}", str(identity.get(key, "")))
        for key in ("commit", "tree")
    ):
        raise VerificationError("The candidate must identify the Engine commit and tree.")
    classification = candidate.get("classification")
    if classification not in ("public", "private-internal"):
        raise VerificationError(f"Unsupported kit classification: {classification}")
    overlay = candidate.get("privateOverlay")
    if classification == "private-internal":
        _check_private_overlay(overlay)
    elif overlay is not None:
        raise VerificationError("A public candidate must not carry a private overlay identity.")
    target_os = candidate.get("target", {}).get("os")
    if target_os != host_os_name():
        raise VerificationError(
            f"This kit targets {target_os}; the verifying host is {host_os_name()}.")
    verify_checksums(kit_root, candidate)

    artifacts = {item["type"]: kit_root / item["path"] for item in candidate["artifacts"]}
    if "cmake-package" not in artifacts or "source-bundle" not in artifacts:
        raise VerificationError("The candidate must contain a CMake package and consumer source bundle.")

    with tempfile.TemporaryDirectory(prefix="wse-offline-accept-") as temporary:
        root = Path(temporary)
        package_root = root / "package"
        consumer_root = root / "consumer"
        build_root = root / "build"
        package_root.mkdir()
        consumer_root.mkdir()
        _safe_extract(artifacts["cmake-package"], package_root)
        _safe_extract(artifacts["source-bundle"], consumer_root)
        _scan_config_for_remote_access(package_root)

        environment = dict(os.environ)
        environment.update(
            {
                "HTTP_PROXY": "http://127.0.0.1:9",
                "HTTPS_PROXY": "http://127.0.0.1:9",
                "ALL_PROXY": "http://127.0.0.1:9",
                "NO_PROXY": "",
            }
        )
        configure_command = [cmake, "-S", str(consumer_root), "-B", str(build_root)]
        if host_os_name() == "windows":
            platforms = {"x86_64": "x64", "arm64": "ARM64"}
            architecture = candidate["target"].get("architecture", "")
            if architecture not in platforms:
                raise VerificationError(f"Unsupported target architecture: {architecture}")
            configure_command += ["-A", platforms[architecture]]
        else:
            # Single-configuration generators take the configuration at configure time; the
            # later --config/-C arguments are accepted and ignored there.
            configure_command += [f"-DCMAKE_BUILD_TYPE={configuration}"]
        configure_command += [
            f"-DWSE_PACKAGE_ROOT={package_root}",
            "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
            "-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON",
        ]
        _run(configure_command, environment)
        _run([cmake, "--build", str(build_root), "--config", configuration], environment)
        cmake_path = Path(cmake)
        if cmake_path.name.lower() == "cmake.exe":
            ctest = str(cmake_path.with_name("ctest.exe"))
        elif cmake_path.name == "cmake" and cmake_path.parent != Path("."):
            ctest = str(cmake_path.with_name("ctest"))
        else:
            ctest = "ctest"
        _run([ctest, "--test-dir", str(build_root), "-C", configuration, "--output-on-failure"], environment)

    manifest = {
        "schemaVersion": 2,
        "kitId": candidate["kitId"],
        "classification": candidate["classification"],
        "createdAt": dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "target": candidate["target"],
        "wse": candidate["wse"],
        "provider": candidate["provider"],
        "artifacts": candidate["artifacts"],
        "dependencies": [_schema_dependency(item) for item in candidate["dependencies"]],
        "verification": {
            "networkDisabled": True,
            "configure": "PASS",
            "build": "PASS",
            "test": "PASS",
            "run": "PASS",
            "checksum": "PASS",
            "remoteAttemptCount": 0,
        },
    }
    if classification == "private-internal":
        manifest["privateOverlay"] = {
            key: overlay[key]
            for key in ("repository", "commit", "tag", "tree", "version", "requiresWse")
            if key in overlay
        }
    output_manifest.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return output_manifest


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kit-root", type=Path, required=True)
    parser.add_argument("--output-manifest", type=Path)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--configuration", default="Debug")
    # --confirm-clean-windows predates the Linux hosts and stays as the compatible spelling.
    parser.add_argument("--confirm-clean-host", "--confirm-clean-windows",
                        dest="confirm_clean_host", action="store_true")
    arguments = parser.parse_args(argv)
    output = arguments.output_manifest or arguments.kit_root / "offline-kit-manifest.json"
    try:
        accepted = accept_kit(
            arguments.kit_root.resolve(),
            output.resolve(),
            arguments.cmake,
            arguments.configuration,
            arguments.confirm_clean_host,
        )
        print(f"[wse-offline-kit] Accepted manifest: {accepted}")
        return 0
    except VerificationError as exc:
        print(f"[wse-offline-kit] ERROR: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
