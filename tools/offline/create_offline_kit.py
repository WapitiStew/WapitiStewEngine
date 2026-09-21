#!/usr/bin/env python3
"""Create an immutable offline-kit candidate from one installed WSE package.

Two classifications exist. A public candidate carries a Core/XPT-only package and passes the
public export denylist. A controlled candidate additionally carries the access-controlled VPJ
overlay, is classified private-internal, and must never travel through a public channel.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence


WSE_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONSUMER = WSE_ROOT / "example" / "offline_consumer"
# Concrete private identifiers live in an external inspection policy. Built-in structural
# checks and invented test markers are available offline without exposing that dictionary.
# A candidate is not publication approval: the full private policy and human review are
# required at the publication boundary. External rules always add to these checks.
DENYLIST = Path(os.environ.get("WSE_EXPORT_DENYLIST", "")) if os.environ.get(
    "WSE_EXPORT_DENYLIST") else None
BUILT_IN_BLOCKED_PATH_REGEXES = [
    r"(^|/)api/vpj(/|$)",
    r"(^|/)core/vpj(/|$)",
    r"(^|/)platform/vpj(/|$)",
    r"(^|/)api/link/vpj_link.h$",
    r"(^|/)doc/report(/|$)",
    r"(^|/)example/[^/]+/vpj(/|$)",
    r"(^|/)lang/[^/]+/native/[^/]*vpj[^/]*$",
    r"(^|/)api/wse/capi/wse_capi_vpj.h$",
    r"(^|/)test/binding/.*vpj.*$",
    r"(^|/).*private-test-marker-1.*$",
    r"(^|/)lib.zip$",
    r"(^|/)doxy/Lib.zip$",
    r".aps$",
    r".vcxproj.user$",
]
# A controlled kit exists to carry the material the public denylist refuses, so that gate does
# not apply to it. What still never ships in any kit is build residue; this is the hygiene
# subset of the export denylist that stays meaningful when the payload itself is controlled.
BLOCKED_BUILD_ARTIFACT_REGEXES = [
    r"\.pdb$",
    r"\.ilk$",
    r"\.exp$",
    r"\.aps$",
    r"\.vcxproj\.user$",
    r"(^|/)lib\.zip$",
    r"(^|/)doxy/Lib\.zip$",
]
# The prefix is an operational control: no controlled artifact may be mistakable for a public
# one by name alone, on a share, in a manifest, or on removable media.
CONTROLLED_KIT_ID_PREFIX = "wse-confidential-"


class KitError(RuntimeError):
    """An actionable offline-kit creation failure."""


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
        raise KitError(f"Cannot read JSON file {path}: {exc}") from exc


def _run_git(arguments: Sequence[str], repository: Path = WSE_ROOT) -> str:
    completed = subprocess.run(
        ["git", "-C", str(repository), *arguments],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        raise KitError(completed.stderr.strip() or "Git identity lookup failed.")
    return completed.stdout.strip()


def source_identity(allow_dirty: bool = False, repository: Path = WSE_ROOT) -> Dict[str, str]:
    if not allow_dirty:
        for arguments in (("diff", "--quiet", "--", "."), ("diff", "--cached", "--quiet", "--", ".")):
            completed = subprocess.run(["git", "-C", str(repository), *arguments], check=False)
            if completed.returncode != 0:
                raise KitError(f"Refusing to identify an offline kit from an uncommitted tree: {repository}")
    repository_root = Path(_run_git(("rev-parse", "--show-toplevel"), repository))
    relative = repository.resolve().relative_to(repository_root).as_posix()
    commit = _run_git(("rev-parse", "HEAD"), repository)
    # `HEAD:.` is not a valid tree-ish spelling when the directory is the repository root itself.
    tree = _run_git(("rev-parse", "HEAD:./" if relative == "." else f"HEAD:{relative}"), repository)
    return {"commit": commit, "tree": tree}


def _compiled_denylist() -> tuple[List[re.Pattern[str]], List[bytes]]:
    # The overlay list adds to the built-in one; it never replaces it. A gate that a caller can
    # weaken by pointing an environment variable at an empty or unreadable file is not a gate.
    patterns = list(BUILT_IN_BLOCKED_PATH_REGEXES) + list(BLOCKED_BUILD_ARTIFACT_REGEXES)
    external_tokens = []
    if DENYLIST is not None:
        if not DENYLIST.is_file():
            raise KitError(f"WSE_EXPORT_DENYLIST does not name a file: {DENYLIST}")
        data = _read_json(DENYLIST)
        overlay = data.get("blocked_path_regexes", [])
        if not overlay:
            raise KitError(f"Denylist has no blocked_path_regexes: {DENYLIST}")
        patterns.extend(overlay)
        external_tokens = data.get("blocked_text_tokens", [])
        if not isinstance(external_tokens, list) or any(
                not isinstance(value, str) or not value for value in external_tokens):
            raise KitError("Invalid external blocked_text_tokens.")
    # Every entry is matched case-insensitively. The private list carries a case_insensitive flag
    # and has always been true; honouring a false would make the gate weaker than the built-in one.
    path_patterns = [re.compile(pattern, re.IGNORECASE) for pattern in patterns]
    # Component names and availability keys are public API (the same boundary as
    # test/package/verify_public_package.py). Device/protocol identity remains blocked;
    # controlled source paths and build residue are independently rejected above.
    blocked_bytes = [
        b"private-test-marker-1",
        b"private-test-marker-2",
        b"private-test-marker-3",
        b"private-test-marker-4",
        b"private-test-marker-5",
        b"private-test-marker-6",
        b"private-test-marker-7",
        b"private-test-marker-8",
    ]
    blocked_bytes.extend(value.lower().encode("utf-8") for value in external_tokens)
    return path_patterns, blocked_bytes


def _external_text_patterns() -> List[re.Pattern[str]]:
    if DENYLIST is None:
        return []
    values = _read_json(DENYLIST).get("blocked_text_regexes", [])
    if not isinstance(values, list) or any(not isinstance(value, str) or not value for value in values):
        raise KitError("Invalid external blocked_text_regexes.")
    try:
        return [re.compile(value, re.IGNORECASE) for value in values]
    except re.error as error:
        raise KitError("Invalid external text inspection expression.") from error


def _binary_strings(content: bytes) -> Iterable[bytes]:
    """Yield meaningful ASCII/UTF-16LE strings without treating random bytes as text."""
    lowered = content.lower()
    yield from re.findall(rb"[\x20-\x7e]{4,}", lowered)
    for value in re.findall(rb"(?:[\x20-\x7e]\x00){4,}", lowered):
        yield value.replace(b"\x00", b"")


def _contains_blocked_content(content: bytes, blocked_bytes: Sequence[bytes]) -> bool:
    if b"\x00" not in content:
        return any(token in content.lower() for token in blocked_bytes)
    return any(token in value for value in _binary_strings(content) for token in blocked_bytes)


def scan_public_tree(root: Path) -> None:
    path_patterns, blocked_bytes = _compiled_denylist()
    text_patterns = _external_text_patterns()
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        relative = path.relative_to(root).as_posix()
        if any(pattern.search(relative) for pattern in path_patterns):
            raise KitError(f"Public offline package path is blocked: {relative}")
        content = path.read_bytes()
        if _contains_blocked_content(content, blocked_bytes):
            raise KitError(f"Public offline package content contains a blocked private token: {relative}")
        strings = [content.decode("utf-8", errors="replace")]
        if b"\0" in content:
            strings.extend(content[offset:].decode(encoding, errors="ignore")
                           for offset in (0, 1) for encoding in ("utf-16-le", "utf-16-be"))
        if any(pattern.search(value) for pattern in text_patterns for value in strings):
            raise KitError(f"Public offline package content violates the external inspection policy: {relative}")


def scan_controlled_tree(root: Path) -> None:
    patterns = [re.compile(pattern, re.IGNORECASE) for pattern in BLOCKED_BUILD_ARTIFACT_REGEXES]
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        relative = path.relative_to(root).as_posix()
        if any(pattern.search(relative) for pattern in patterns):
            raise KitError(f"Controlled offline package contains build residue: {relative}")


def deterministic_zip(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(source.rglob("*")):
            if path.is_symlink():
                raise KitError(f"Symbolic links are not allowed in an offline kit: {path}")
            if not path.is_file():
                continue
            relative = path.relative_to(source).as_posix()
            info = zipfile.ZipInfo(relative, date_time=(2020, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o644 & 0xFFFF) << 16
            archive.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)


def _artifact(path: Path, root: Path, artifact_type: str, classification: str = "public") -> Dict:
    return {
        "path": path.relative_to(root).as_posix(),
        "type": artifact_type,
        "classification": classification,
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def create_candidate(
    package_root: Path,
    output: Path,
    kit_id: str,
    compiler: str,
    toolchain_version: str,
    identity: Dict[str, str],
    consumer_root: Path = DEFAULT_CONSUMER,
    *,
    classification: str = "public",
    target_os: str = "windows",
    target_arch: str = "x86_64",
    overlay_identity: Optional[Dict[str, str]] = None,
) -> Path:
    package_root = package_root.resolve()
    consumer_root = consumer_root.resolve()
    package_metadata = _read_json(package_root / "wse-package.json")
    for key in ("commit", "tree"):
        if not re.fullmatch(r"[0-9a-fA-F]{40}", identity.get(key, "")):
            raise KitError(f"Engine {key} must be a 40-hex identity.")
    if package_metadata.get("source_commit") != identity["tree"]:
        raise KitError("The installed package does not match the Engine source tree.")
    components = package_metadata.get("components", {})
    has_other_component = any(components.get(name) is True for name in ("gef", "iui", "oui", "tmr"))
    if classification == "public":
        if components.get("xpt") is not True or has_other_component or components.get("vpj") is True:
            raise KitError("The public offline kit accepts a Core/XPT-only package.")
        scan_public_tree(package_root)
        artifact_classification = "public"
    elif classification == "controlled":
        if components.get("xpt") is not True or has_other_component or components.get("vpj") is not True:
            raise KitError("The controlled offline kit accepts a Core/XPT/VPJ-only package.")
        if not kit_id.startswith(CONTROLLED_KIT_ID_PREFIX):
            raise KitError(f"A controlled kit id must start with '{CONTROLLED_KIT_ID_PREFIX}': {kit_id}")
        if overlay_identity is None:
            raise KitError("A controlled kit needs the extension overlay identity; pass --extension-root.")
        # The package self-records the overlay tree it was built from. A candidate whose overlay
        # working copy no longer matches the installed package identifies nothing.
        packaged_overlay_tree = package_metadata.get("extension_commit", "")
        if packaged_overlay_tree != overlay_identity["tree"]:
            raise KitError(
                "The installed package does not match the extension overlay tree: "
                f"package records {packaged_overlay_tree or 'nothing'}, "
                f"overlay is {overlay_identity['tree']}."
            )
        scan_controlled_tree(package_root)
        artifact_classification = "private-internal"
    else:
        raise KitError(f"Unknown kit classification: {classification}")

    if output.exists():
        raise KitError(f"Output already exists: {output}")

    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{output.name}-", dir=str(output.parent)))
    try:
        package_archive = staging / "artifacts" / "wse-package.zip"
        consumer_archive = staging / "artifacts" / "offline-consumer-source.zip"
        deterministic_zip(package_root, package_archive)
        deterministic_zip(consumer_root, consumer_archive)

        artifacts = [
            _artifact(package_archive, staging, "cmake-package", artifact_classification),
            _artifact(consumer_archive, staging, "source-bundle", artifact_classification),
        ]
        dependencies = []
        seen = set()
        for metadata_path in sorted(package_root.rglob("wse-dependency.json")):
            dependency = _read_json(metadata_path)
            key = (dependency.get("name"), dependency.get("version"), dependency.get("recipe_sha256"))
            if key in seen:
                continue
            seen.add(key)
            dependencies.append(
                {
                    "name": dependency["name"],
                    "version": dependency["version"],
                    "license": dependency["license"],
                    "recipe_sha256": dependency.get("recipe_sha256", ""),
                    "artifact": artifacts[0],
                }
            )

        modules = ["core"] + [name for name in ("xpt", "vpj") if components.get(name) is True]
        candidate = {
            "format": "wse-offline-kit-candidate-v2",
            "kitId": kit_id,
            "classification": artifact_classification,
            "target": {
                "os": target_os,
                "architecture": target_arch,
                "compiler": compiler,
                "toolchainVersion": toolchain_version,
            },
            "wse": {
                "repository": "Engine",
                "commit": identity["commit"],
                "tree": identity["tree"],
                "version": package_metadata["version"],
                "modules": modules,
                "buildSharedLibs": package_metadata["library_type"] == "SHARED",
            },
            "provider": {
                "default": "package",
                "allowed": ["package"],
                "remoteAccessAllowed": False,
            },
            "artifacts": artifacts,
            "dependencies": dependencies,
            "acceptance": {
                "status": "NOT_RUN",
                "requiredSchema": "OfflineKitManifest.schema.json",
                "verifier": "verify_offline_kit.py",
            },
        }
        if classification == "controlled":
            # The overlay carries no version of its own; it releases only as part of the WSE
            # package it was installed into, so both version fields state that package version.
            candidate["privateOverlay"] = {
                "repository": "private-overlay",
                "commit": overlay_identity["commit"],
                "tree": overlay_identity["tree"],
                "version": package_metadata["version"],
                "requiresWse": package_metadata["version"],
            }
        shutil.copyfile(Path(__file__).with_name("OfflineKitManifest.schema.json"),
                        staging / "OfflineKitManifest.schema.json")
        shutil.copyfile(Path(__file__).with_name("verify_offline_kit.py"),
                        staging / "verify_offline_kit.py")
        candidate_path = staging / "offline-kit-candidate.json"
        candidate_path.write_text(json.dumps(candidate, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        checksums = [f"{item['sha256']}  {item['path']}" for item in artifacts]
        (staging / "SHA256SUMS").write_text("\n".join(checksums) + "\n", encoding="ascii")
        if classification == "public":
            readme_text = (
                "WSE Core/XPT offline-kit candidate.\n"
                "This is not accepted until verify_offline_kit.py succeeds on a clean target PC "
                "with every network adapter disabled.\n"
            )
        else:
            readme_text = (
                "WSE controlled Core/XPT/VPJ offline-kit candidate (private-internal).\n"
                "Handle under the private-overlay distribution rules; this kit must never "
                "reach a public channel or shared storage.\n"
                "This is not accepted until verify_offline_kit.py succeeds on a clean PC "
                "with every network adapter disabled.\n"
            )
        (staging / "README.txt").write_text(readme_text, encoding="utf-8")
        staging.replace(output)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
    return output / "offline-kit-candidate.json"


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--kit-id", required=True)
    parser.add_argument("--classification", choices=("public", "controlled"), default="public")
    parser.add_argument("--target-os", choices=("windows", "linux"), default="windows")
    parser.add_argument("--target-arch", choices=("x86_64", "arm64"), default="x86_64")
    parser.add_argument("--extension-root", type=Path,
                        help="private-overlay working copy; required for a controlled kit.")
    parser.add_argument("--consumer-root", type=Path,
                        help="Consumer source to bundle; defaults per classification.")
    parser.add_argument("--compiler", default=None,
                        help="Defaults to MSVC for windows targets and GCC for linux targets.")
    parser.add_argument("--toolchain-version", required=True)
    parser.add_argument("--allow-dirty", action="store_true")
    arguments = parser.parse_args(argv)
    compiler = arguments.compiler or ("MSVC" if arguments.target_os == "windows" else "GCC")
    try:
        identity = source_identity(arguments.allow_dirty)
        overlay_identity = None
        consumer_root = arguments.consumer_root
        if arguments.classification == "controlled":
            if arguments.extension_root is None:
                raise KitError("A controlled kit needs --extension-root.")
            extension_root = arguments.extension_root.resolve()
            overlay_identity = source_identity(arguments.allow_dirty, extension_root)
            if consumer_root is None:
                consumer_root = extension_root / "example" / "offline_consumer"
        elif consumer_root is None:
            consumer_root = DEFAULT_CONSUMER
        candidate = create_candidate(
            arguments.package_root,
            arguments.output,
            arguments.kit_id,
            compiler,
            arguments.toolchain_version,
            identity,
            consumer_root,
            classification=arguments.classification,
            target_os=arguments.target_os,
            target_arch=arguments.target_arch,
            overlay_identity=overlay_identity,
        )
        print(f"[wse-offline-kit] Candidate created: {candidate}")
        return 0
    except KitError as exc:
        print(f"[wse-offline-kit] ERROR: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
