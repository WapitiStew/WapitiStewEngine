#!/usr/bin/env python3
"""Prepare WSE third-party dependencies without CMake-time downloads.

The default command may download pinned sources on a connected staging PC.
``--offline`` is strict: it only consumes the selected cache and never falls
back to a remote URL. The script uses only the Python standard library.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import posixpath
import re
import shutil
import ssl
import stat
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath, PureWindowsPath
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


WSE_ROOT = Path(__file__).resolve().parent
DEFAULT_MANIFEST = WSE_ROOT / "bootstrap-manifest.json"
DEFAULT_CACHE = WSE_ROOT / ".bootstrap-cache"
DEFAULT_VENDOR = WSE_ROOT / "vendor"
DEFAULT_USER_PRESETS = WSE_ROOT / "CMakeUserPresets.json"
TOOLCHAIN_STAGING = "toolchain-staging"


# ----------------------------------------------------------------------------------------------
# Verification toolchains and language profiles
#
# A toolchain is a verification tool (a JDK, a Node.js runtime, a CPython interpreter, a .NET
# SDK), not a dependency. WSE never links against it and never ships it, so it is provisioned
# into the gitignored bootstrap cache and never into vendor/ or an install package. That keeps
# the redistribution surface and the SBOM unchanged while still letting one command prepare a
# machine.
#
# Resolution prefers what is already installed. A toolchain is fetched only when detection finds
# nothing that satisfies the pinned minimum version, and --no-fetch-toolchain turns fetching off
# entirely so a CI image always uses its own toolchain.
# ----------------------------------------------------------------------------------------------

TOOLCHAIN_SUBDIR = "toolchains"


def _executable_candidates(name: str) -> List[str]:
    """The file names one tool can carry on this host.

    A `.exe` candidate exists only on Windows. Under WSL the Windows directories are on PATH, so
    accepting `dotnet.exe` on Linux would select a Windows executable to drive a Linux build.
    """
    if os.name != "nt":
        return [name]
    return [name, f"{name}.exe", f"{name}.cmd", f"{name}.bat"]


def _executable_names(spec: Dict) -> List[str]:
    """The names one tool is installed under; Ubuntu ships CPython as `python3`, not `python`."""
    names = [spec["executable"]] if spec.get("executable") else []
    names.extend(spec.get("executable_alternatives", []))
    return names


def _read_version_output(executable: Path, arguments: Sequence[str]) -> str:
    try:
        completed = subprocess.run(
            [str(executable), *arguments],
            check=False,
            capture_output=True,
            text=True,
            timeout=60,
        )
    except (OSError, subprocess.SubprocessError):
        return ""
    return f"{completed.stdout}\n{completed.stderr}"


def _parse_version(text: str, spec: Dict) -> Optional[Tuple[int, ...]]:
    pattern = spec.get("version_pattern")
    if not pattern:
        return None
    matches = list(re.finditer(pattern, text))
    if not matches:
        return None
    # `--list-sdks` prints every installed SDK, so the newest entry decides whether the
    # requirement is met rather than whichever line happens to come first.
    match = matches[-1] if spec.get("version_selects_maximum") else matches[0]
    try:
        return tuple(int(group) for group in match.groups() if group is not None)
    except ValueError:
        return None


def _version_satisfies(version: Optional[Tuple[int, ...]], spec: Dict) -> bool:
    minimum_major = spec.get("minimum_major")
    if minimum_major is None:
        return True
    if not version:
        return False
    if version[0] != minimum_major:
        return version[0] > minimum_major
    minimum_minor = spec.get("minimum_minor")
    if minimum_minor is None:
        return True
    return len(version) > 1 and version[1] >= minimum_minor


def _executable_for_root(root: Path, spec: Dict) -> Optional[Path]:
    subpath = spec.get("executable_subpath")
    if subpath:
        candidate = root / subpath
        if candidate.is_file():
            return candidate
    for directory in (root, root / "bin"):
        for name in _executable_names(spec):
            for candidate_name in _executable_candidates(name):
                candidate = directory / candidate_name
                if candidate.is_file():
                    return candidate
    return None


def _candidate_roots(spec: Dict) -> List[Path]:
    """Yields every place a toolchain may already exist, in decreasing order of intent."""
    roots: List[Path] = []
    for variable in spec.get("environment_roots", []):
        value = os.environ.get(variable)
        if value:
            roots.append(Path(value))

    if spec.get("include_running_python"):
        # The interpreter running bootstrap is itself a CPython install, and on Windows it is often
        # the only one that is not the non-functional Microsoft Store stub on PATH.
        roots.append(Path(sys.executable).resolve().parent)

    for name in _executable_names(spec):
        for candidate_name in _executable_candidates(name):
            found = shutil.which(candidate_name)
            if not found:
                continue
            executable = Path(found).resolve()
            # A tool on PATH lives either directly in its root or in its `bin` directory.
            roots.append(executable.parent)
            roots.append(executable.parent.parent)
    return roots


def detect_toolchain(toolchain: Dict) -> Optional[Tuple[Path, Path, Optional[Tuple[int, ...]]]]:
    """Returns the (root, executable, version) of an installed toolchain that meets the minimum."""
    spec = toolchain.get("detect", {})
    if not spec:
        return None
    for root in _candidate_roots(spec):
        executable = _executable_for_root(root, spec)
        if executable is None:
            continue
        version = _parse_version(_read_version_output(executable, spec.get("version_arguments", [])), spec)
        if _version_satisfies(version, spec):
            return root, executable, version
    return None


def _toolchain_host_source(toolchain: Dict) -> Optional[Dict]:
    """Returns the pinned archive for this host, or None when the toolchain cannot be fetched.

    A verification tool always runs on the machine that builds, so it is selected by host platform
    and host architecture even during a cross build. `--target-architecture` selects the dependency
    artifacts WSE links against; it must never pick a compiler or interpreter this host cannot run.
    """
    entry = toolchain.get("platforms", {}).get(_host_platform())
    if not entry:
        return None
    return entry.get("architectures", {}).get(_host_architecture())


def _toolchain_requirement(toolchain: Dict) -> str:
    """Describes what would satisfy a toolchain, which is a minimum and not the pinned version."""
    spec = toolchain.get("detect", {})
    major = spec.get("minimum_major")
    if major is None:
        return toolchain["version"]
    minor = spec.get("minimum_minor")
    minimum = f"{major}.{minor}" if minor is not None else str(major)
    return f"{minimum} or newer (pinned {toolchain['version']})"


def _toolchain_root(toolchain: Dict, cache_dir: Path) -> Path:
    subdir = toolchain.get("install_subdir", toolchain["name"])
    return cache_dir / TOOLCHAIN_SUBDIR / subdir


def _toolchain_required_files(toolchain: Dict) -> List[str]:
    """The files that must exist for this host; a JDK names `bin/javac.exe` only on Windows."""
    source = _toolchain_host_source(toolchain)
    if source is not None and "required_files" in source:
        return list(source["required_files"])
    return list(toolchain.get("required_files", []))


def _toolchain_missing_files(toolchain: Dict, root: Path) -> List[str]:
    return [name for name in _toolchain_required_files(toolchain) if not (root / name).is_file()]


def _selected_toolchains(manifest: Dict, names: Iterable[str]) -> List[Dict]:
    requested = list(names)
    available = {entry["name"]: entry for entry in manifest.get("toolchains", [])}
    unknown = sorted(set(requested) - set(available))
    if unknown:
        raise BootstrapError(f"Unknown toolchain name(s): {', '.join(unknown)}")
    ordered: List[Dict] = []
    for name in requested:
        if available[name] not in ordered:
            ordered.append(available[name])
    return ordered


def _selection_target(target_architecture: Optional[str] = None) -> str:
    """The `<platform>/<architecture>` key a profile uses for target-specific additions."""
    return f"{_host_platform()}/{_normalize_architecture(target_architecture)}"


def _resolve_selection(
    manifest: Dict,
    languages: Sequence[str],
    components: Sequence[str],
    target: str,
) -> Tuple[List[str], List[str], Dict[str, str]]:
    """Expands a language and component selection into packages, toolchains, and CMake options.

    A profile may add packages for one target only. Linux ARM64 XPT needs the pinned OpenSSL that
    its libcurl links against, while no other target does, and selecting a package a target has no
    entry for is an error rather than a silent skip.
    """
    language_profiles = manifest.get("language_profiles", {})
    component_profiles = manifest.get("component_profiles", {})

    unknown_languages = sorted(set(languages) - set(language_profiles))
    if unknown_languages:
        raise BootstrapError(
            f"Unknown language(s): {', '.join(unknown_languages)}; "
            f"known languages are {', '.join(sorted(language_profiles))}."
        )
    unknown_components = sorted(set(components) - set(component_profiles))
    if unknown_components:
        raise BootstrapError(
            f"Unknown component(s): {', '.join(unknown_components)}; "
            f"known components are {', '.join(sorted(component_profiles))}."
        )

    packages: List[str] = []
    toolchains: List[str] = []
    cmake: Dict[str, str] = {}

    def append_unique(target: List[str], values: Iterable[str]) -> None:
        for value in values:
            if value not in target:
                target.append(value)

    def visit_component(name: str, seen: set) -> None:
        if name in seen:
            return
        seen.add(name)
        profile = component_profiles[name]
        # Expand implications first so a transitive component contributes its packages too.
        for implied in profile.get("implies", []):
            visit_component(implied, seen)
        append_unique(packages, profile.get("packages", []))
        append_unique(packages, profile.get("packages_by_target", {}).get(target, []))
        cmake.update(profile.get("cmake", {}))

    seen_components: set = set()
    for name in components:
        visit_component(name, seen_components)

    for name in languages:
        profile = language_profiles[name]
        append_unique(packages, profile.get("packages", []))
        append_unique(packages, profile.get("packages_by_target", {}).get(target, []))
        append_unique(toolchains, profile.get("toolchains", []))
        cmake.update(profile.get("cmake", {}))

    return packages, toolchains, cmake


def _toolchain_cmake_values(toolchain: Dict, root: Path, executable: Optional[Path]) -> Dict[str, str]:
    """Substitutes the resolved paths into the CMake cache variables a toolchain provides."""
    resolved: Dict[str, str] = {}
    for variable, template in toolchain.get("cmake", {}).items():
        value = template.replace("{root}", root.as_posix())
        if executable is not None:
            value = value.replace("{executable}", executable.as_posix())
        elif "{executable}" in value:
            continue
        resolved[variable] = value
    return resolved


def _package_cmake_values(
    package: Dict,
    vendor_root: Path,
    target_architecture: Optional[str],
) -> Dict[str, str]:
    """Pins the dependency roots CMake cannot infer for this target.

    CMake derives most dependency directories from `WSE_DEPENDENCY_ROOT`, but a few
    architecture-specific installs do not sit where that default looks. The manifest names the cache
    variable for exactly those, and bootstrap fills in where it actually installed the package.
    """
    resolved = _resolve_platform(package, target_architecture)
    template = resolved.get("cmake")
    if not template:
        return {}
    target_dir = _target_for(resolved, vendor_root).as_posix()
    return {
        variable: value.replace("{target_dir}", target_dir)
        for variable, value in template.items()
    }


def _write_user_presets(
    path: Path,
    preset_name: str,
    base_preset: str,
    cache_variables: Dict[str, str],
    description: str,
) -> Path:
    """Writes a user-local preset that inherits an official preset and adds the resolved settings.

    `CMakeUserPresets.json` is CMake's own user-local mechanism and is never committed, so the
    generated machine-specific paths stay out of the repository.
    """
    document = {
        "version": 6,
        "cmakeMinimumRequired": {"major": 3, "minor": 25, "patch": 0},
        "configurePresets": [
            {
                "name": preset_name,
                "inherits": base_preset,
                "displayName": f"WSE local ({base_preset})",
                "description": description,
                "cacheVariables": dict(sorted(cache_variables.items())),
            }
        ],
    }
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    return path


class BootstrapError(RuntimeError):
    """An actionable dependency provisioning failure."""


def _message(text: str) -> None:
    print(f"[wse-bootstrap] {text}")


def _load_manifest(path: Path) -> Dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise BootstrapError(f"Cannot read dependency manifest '{path}': {exc}") from exc
    if data.get("schema_version") != 2 or not isinstance(data.get("packages"), list):
        raise BootstrapError(f"Unsupported dependency manifest schema in '{path}'.")
    return data


def _sha256(path: Path) -> str:
    return _file_digest(path, "sha256")


# Vendors publish the digest they publish: Node.js and Adoptium sign SHA-256 sums, while the .NET
# release metadata and the NuGet catalog publish SHA-512. Accepting both lets every pinned archive
# carry the vendor's own digest instead of one this repository computed for itself.
DIGEST_ALGORITHMS = ("sha256", "sha512")
DIGEST_LABELS = {"sha256": "SHA-256", "sha512": "SHA-512"}


def _file_digest(path: Path, algorithm: str) -> str:
    digest = hashlib.new(algorithm)
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _pinned_digests(source: Dict) -> List[Tuple[str, str]]:
    """Returns every digest a source pins, so an archive that pins both must satisfy both."""
    digests = [
        (algorithm, str(source[algorithm]).lower())
        for algorithm in DIGEST_ALGORITHMS
        if source.get(algorithm)
    ]
    if not digests:
        raise BootstrapError(
            f"{source.get('name', '<unnamed>')} {source.get('version', '')} pins no archive digest; "
            f"one of {', '.join(DIGEST_ALGORITHMS)} is required."
        )
    return digests


def _check_digests(source: Dict, archive: Path, context: str) -> None:
    for algorithm, expected in _pinned_digests(source):
        actual = _file_digest(archive, algorithm)
        if actual != expected:
            raise BootstrapError(
                f"Checksum mismatch for {source['name']} {source['version']}: {context}; "
                f"expected {DIGEST_LABELS[algorithm]} {expected}, actual {actual}."
            )


def _host_platform() -> str:
    if sys.platform == "win32":
        return "windows"
    if sys.platform.startswith("linux"):
        return "linux"
    return sys.platform.lower()


def _host_architecture() -> str:
    machine = platform.machine().lower()
    if machine in {"amd64", "x86_64"}:
        return "x86_64"
    if machine in {"aarch64", "arm64"}:
        return "arm64"
    return machine or "unknown"


def _normalize_architecture(value: Optional[str]) -> str:
    architecture = (value or _host_architecture()).lower()
    if architecture in {"amd64", "x86_64"}:
        return "x86_64"
    if architecture in {"aarch64", "arm64"}:
        return "arm64"
    return architecture


def _merge_target_override(package: Dict, override: Dict) -> Dict:
    resolved = dict(package)
    for field, value in override.items():
        if field not in {"config", "required_files", "required_globs", "architectures"}:
            resolved[field] = value
    for field in ("required_files", "required_globs"):
        resolved[field] = list(package.get(field, [])) + list(override.get(field, []))

    config = dict(package.get("config", {}))
    config.update(override.get("config", {}))
    resolved["config"] = config
    if "architectures" in override:
        resolved["architectures"] = override["architectures"]
    return resolved


def _resolve_platform(package: Dict, target_architecture: Optional[str] = None) -> Dict:
    platforms = package.get("platforms")
    if not platforms:
        return package

    platform_name = _host_platform()
    override = platforms.get(platform_name)
    if not isinstance(override, dict):
        supported = ", ".join(sorted(platforms))
        raise BootstrapError(
            f"{package['name']} does not support bootstrap provisioning on {platform_name}; "
            f"supported platforms: {supported}."
        )

    resolved = _merge_target_override(package, override)
    resolved.pop("platforms", None)
    resolved["resolved_platform"] = platform_name

    architecture_name = _normalize_architecture(target_architecture)
    architectures = resolved.get("architectures")
    if architectures:
        architecture_override = architectures.get(architecture_name)
        if not isinstance(architecture_override, dict):
            supported = ", ".join(sorted(architectures))
            raise BootstrapError(
                f"{package['name']} does not support bootstrap provisioning for {architecture_name}; "
                f"supported architectures: {supported}."
            )
        resolved = _merge_target_override(resolved, architecture_override)
    resolved.pop("architectures", None)
    resolved["resolved_architecture"] = architecture_name
    return resolved


def _dependency_metadata(package: Dict) -> Dict:
    source = package.get("source", {})
    signature = package.get("signature", {})
    recipe = {
        "config": package.get("config", {}),
        "install_subdir": package.get("install_subdir", ""),
        "linkage": package.get("linkage", "unspecified"),
        "required_files": package.get("required_files", []),
        "required_globs": package.get("required_globs", []),
        "supplemental_files": package.get("supplemental_files", []),
        "target": {
            "platform": package.get("resolved_platform", _host_platform()),
            "architecture": package.get("resolved_architecture", _host_architecture()),
        },
    }
    recipe_sha256 = hashlib.sha256(
        json.dumps(recipe, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    return {
        "schema_version": 2,
        "name": package["name"],
        "version": package["version"],
        "license": package["license"],
        "license_file": package["license_file"],
        "source": {
            "url": package.get("url", source.get("url", "")),
            "archive_name": package.get("archive_name", ""),
            "sha256": package.get("sha256", ""),
            "signature_file": signature.get("file", ""),
            "signer_fingerprint": signature.get("signer_fingerprint", ""),
        },
        "target": {
            "platform": package.get("resolved_platform", _host_platform()),
            "architecture": package.get("resolved_architecture", _host_architecture()),
            "linkage": package.get("linkage", "unspecified"),
        },
        "recipe_sha256": recipe_sha256,
        "system_dependencies": package.get("system_dependencies", []),
    }


def _required_missing(package: Dict, target: Path) -> List[str]:
    missing = [name for name in package.get("required_files", []) if not (target / name).is_file()]
    missing.extend(
        pattern for pattern in package.get("required_globs", []) if not any(target.glob(pattern))
    )
    metadata_file = package.get("metadata_file")
    if metadata_file:
        metadata_path = target / metadata_file
        if not metadata_path.is_file():
            missing.append(metadata_file)
        else:
            try:
                actual = json.loads(metadata_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                actual = None
            if actual != _dependency_metadata(package):
                missing.append(f"{metadata_file} (identity mismatch)")
    return missing


def _target_for(package: Dict, vendor_root: Path) -> Path:
    subdir = package.get("install_subdir")
    if not isinstance(subdir, str) or not subdir or Path(subdir).is_absolute() or ".." in Path(subdir).parts:
        raise BootstrapError(f"Invalid install_subdir for {package.get('name', 'unknown')}: {subdir!r}")
    return vendor_root / subdir


def _run(command: Sequence[str], cwd: Optional[Path] = None, capture: bool = False) -> subprocess.CompletedProcess:
    rendered = " ".join(str(item) for item in command)
    _message(f"Run: {rendered}")
    return subprocess.run(
        [str(item) for item in command],
        cwd=str(cwd) if cwd else None,
        check=False,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
    )


def _git_head(checkout: Path) -> Optional[str]:
    git = shutil.which("git")
    if not git or not (checkout / ".git").exists():
        return None
    completed = _run([git, "-C", str(checkout), "rev-parse", "HEAD"], capture=True)
    if completed.returncode != 0:
        return None
    return completed.stdout.strip().lower()


def _git_checkout_valid(package: Dict, checkout: Path) -> bool:
    expected = str(package["source"].get("commit", "")).lower()
    return bool(expected) and _git_head(checkout) == expected


def _verify_archive_digest(package: Dict, archive: Path) -> None:
    if not archive.is_file():
        pinned = ", ".join(
            f"{DIGEST_LABELS[algorithm]} {expected}" for algorithm, expected in _pinned_digests(package)
        )
        raise BootstrapError(
            f"Offline cache is missing {package['name']} {package['version']}: {archive} "
            f"(expected {pinned})."
        )
    _check_digests(package, archive, str(archive))


def _signature_path(package: Dict, cache_dir: Path) -> Optional[Path]:
    signature = package.get("signature")
    if not signature:
        return None
    file_name = signature.get("file")
    if not isinstance(file_name, str) or not file_name or Path(file_name).name != file_name:
        raise BootstrapError(f"Invalid signature file for {package['name']}: {file_name!r}")
    return cache_dir / file_name


def _verify_signature_cache(package: Dict, cache_dir: Path) -> Optional[Path]:
    signature_path = _signature_path(package, cache_dir)
    if signature_path is None:
        return None
    if not signature_path.is_file():
        raise BootstrapError(
            f"Offline cache is missing the release signature for {package['name']} "
            f"{package['version']}: {signature_path}."
        )
    try:
        signature_text = signature_path.read_text(encoding="ascii")
    except (OSError, UnicodeError) as exc:
        raise BootstrapError(f"Cannot read cached release signature as ASCII: {signature_path}") from exc
    if "-----BEGIN PGP SIGNATURE-----" not in signature_text:
        raise BootstrapError(f"Cached release signature is not ASCII-armored OpenPGP: {signature_path}")
    return signature_path


def _verify_archive(package: Dict, archive: Path, cache_dir: Path) -> None:
    _verify_archive_digest(package, archive)
    _verify_signature_cache(package, cache_dir)


def _supplemental_cache_path(package: Dict, item: Dict, cache_dir: Path) -> Path:
    file_name = item.get("file")
    if not isinstance(file_name, str) or not file_name or Path(file_name).name != file_name:
        raise BootstrapError(
            f"Invalid supplemental cache file for {package['name']}: {file_name!r}"
        )
    return cache_dir / file_name


def _verify_supplemental_files(package: Dict, cache_dir: Path) -> List[Path]:
    verified = []
    for item in package.get("supplemental_files", []):
        cached = _supplemental_cache_path(package, item, cache_dir)
        expected = str(item.get("sha256", "")).lower()
        if not cached.is_file():
            raise BootstrapError(
                f"Offline cache is missing a supplemental file for {package['name']} "
                f"{package['version']}: {cached} (expected SHA-256 {expected})."
            )
        actual = _sha256(cached)
        if not expected or actual != expected:
            raise BootstrapError(
                f"Checksum mismatch for supplemental file {cached}; "
                f"expected {expected or '<manifest value missing>'}, actual {actual}."
            )
        verified.append(cached)
    return verified


def _verify_git_cache(package: Dict, cache_dir: Path) -> Tuple[Path, str]:
    source = package["source"]
    checkout = cache_dir / source["checkout_dir"]
    expected = str(source.get("commit", "")).lower()
    if _git_checkout_valid(package, checkout):
        return checkout, f"checkout {expected}"

    bundle_name = source.get("bundle")
    bundle = cache_dir / bundle_name if bundle_name else None
    if bundle and bundle.is_file():
        git = shutil.which("git")
        if not git:
            raise BootstrapError(f"Git is required to inspect cached bundle: {bundle}")
        completed = _run([git, "bundle", "list-heads", str(bundle)], capture=True)
        if completed.returncode == 0 and expected in completed.stdout.lower():
            return bundle, f"bundle containing commit {expected}"
        raise BootstrapError(
            f"Git bundle does not contain the pinned commit for {package['name']} {package['version']}: "
            f"{bundle}; expected {expected}."
        )

    raise BootstrapError(
        f"Offline cache is missing {package['name']} {package['version']}. Expected checkout "
        f"'{checkout}' at commit {expected}"
        + (f" or bundle '{bundle}'." if bundle else ".")
    )


def verify_cache(package: Dict, cache_dir: Path) -> str:
    source = package.get("source", {})
    if source.get("type") == "git":
        path, identity = _verify_git_cache(package, cache_dir)
        return f"{path} ({identity})"

    archive = cache_dir / package["archive_name"]
    _verify_archive(package, archive, cache_dir)
    supplemental = _verify_supplemental_files(package, cache_dir)
    signature = _signature_path(package, cache_dir)
    signature_text = f", signature {signature.name}" if signature else ""
    supplemental_text = (
        ", supplemental " + ", ".join(path.name for path in supplemental)
        if supplemental
        else ""
    )
    pinned = ", ".join(
        f"{DIGEST_LABELS[algorithm]} {expected}" for algorithm, expected in _pinned_digests(package)
    )
    return f"{archive} ({pinned}{signature_text}{supplemental_text})"


def _validate_archive_member(archive: Path, destination_root: Path, member_name: str) -> None:
    member_path = (destination_root / member_name).resolve()
    try:
        member_path.relative_to(destination_root.resolve())
    except ValueError as exc:
        raise BootstrapError(f"Unsafe archive member in {archive}: {member_name}") from exc


def _validate_archive_link(
    archive: Path,
    destination_root: Path,
    member_name: str,
    link_name: str,
    relative_to_member: bool,
) -> None:
    """Rejects a link whose target could resolve outside the extracted tree.

    A symbolic link resolves against the directory holding it; a hard link resolves against the
    archive root. Either way the resolved target must stay inside the destination.
    """
    if not link_name:
        raise BootstrapError(f"Empty link target in {archive}: {member_name}")
    if PurePosixPath(link_name).is_absolute() or PureWindowsPath(link_name).is_absolute():
        raise BootstrapError(f"Absolute link target in {archive}: {member_name} -> {link_name}")
    base = PurePosixPath(member_name).parent if relative_to_member else PurePosixPath(".")
    _validate_archive_member(archive, destination_root, str(base / link_name))


def _extended_length_path(path: Path) -> Path:
    """Returns a path Windows accepts beyond its 260-character limit.

    The pinned .NET SDK archive contains member paths that exceed the limit on their own, so the
    extraction target carries the prefix. Every path this script reports stays the readable form.
    """
    if os.name != "nt":
        return path
    text = str(path.resolve())
    if text.startswith("\\\\?\\"):
        return path
    if text.startswith("\\\\"):
        return Path("\\\\?\\UNC\\" + text[2:])
    return Path("\\\\?\\" + text)


def _requires_extended_length_path(destination: Path, member_names: Iterable[str]) -> bool:
    """Returns whether extracting members below destination exceeds Windows path limits."""
    if os.name != "nt":
        return False
    destination_text = str(destination.resolve())
    return any(
        len(destination_text + os.sep + member_name.replace("/", os.sep)) >= 260
        for member_name in member_names
    )


def _remove_tree(path: Path) -> None:
    """Deletes a bootstrap-managed tree, including files an archive marked read-only.

    A Linux archive stores mode 0444 for its licence files, which Windows materializes as the
    read-only attribute; `shutil.rmtree` then cannot unlink them. Replacing a cached toolchain has
    to succeed regardless, so the attribute is cleared and the removal retried.
    """

    def _clear_read_only(function, target, _exception):
        os.chmod(target, stat.S_IWRITE)
        function(target)

    target = _extended_length_path(path)
    if sys.version_info >= (3, 12):
        shutil.rmtree(target, onexc=_clear_read_only)
    else:
        shutil.rmtree(target, onerror=_clear_read_only)


def _safe_extract_archive(
    archive: Path,
    destination: Path,
    *,
    allow_internal_links: bool = False,
) -> None:
    """Extracts an archive, rejecting any member that could write outside the destination.

    A dependency is extracted strictly. It is copied into ``vendor/`` and redistributed, so a link
    is never expected there. A verification toolchain is an upstream distribution that does use
    internal relative links - Node.js points `bin/npm` into `lib/node_modules`, and a JDK points its
    per-module licence files at a shared copy - so ``allow_internal_links`` permits a link whose
    target is validated to stay inside the extracted tree.
    """
    destination.mkdir(parents=True, exist_ok=True)
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as source:
            members = source.infolist()
            for member in members:
                _validate_archive_member(archive, destination, member.filename)
                # A zip stores a link target as file content, so it cannot be checked up front.
                if stat.S_ISLNK(member.external_attr >> 16):
                    raise BootstrapError(f"Unsupported archive member in {archive}: {member.filename}")
            target = (
                _extended_length_path(destination)
                if _requires_extended_length_path(destination, (member.filename for member in members))
                else destination
            )
            source.extractall(target)
        return

    try:
        with tarfile.open(archive, mode="r:*") as source:
            members = []
            link_members = []
            for member in source.getmembers():
                # An extended-length path is used literally, so a member named `./` would create a
                # directory called `.`. Normalizing first keeps the check and the write in step.
                name = posixpath.normpath(member.name)
                if name in (".", "/"):
                    continue
                member.name = name
                _validate_archive_member(archive, destination, name)
                if member.isdev():
                    raise BootstrapError(f"Unsupported archive member in {archive}: {name}")
                if member.issym() or member.islnk():
                    if not allow_internal_links:
                        raise BootstrapError(f"Unsupported archive member in {archive}: {name}")
                    if member.islnk():
                        member.linkname = posixpath.normpath(member.linkname)
                    _validate_archive_link(
                        archive, destination, name, member.linkname, member.issym()
                    )
                    # A link is materialized after every regular member exists, because its
                    # target may appear later in the archive and a host without link support
                    # falls back to copying that target.
                    link_members.append(member)
                    continue
                members.append(member)
            # Python's tarfile normalizes child paths internally. Passing the extended-length
            # prefix for ordinary paths can produce mixed-separator paths rejected by Windows.
            target = (
                _extended_length_path(destination)
                if _requires_extended_length_path(
                    destination, (member.name for member in members + link_members)
                )
                else destination
            )
            if sys.version_info >= (3, 12):
                source.extractall(target, members=members, filter="fully_trusted")
            else:
                source.extractall(target, members=members)
            for member in link_members:
                _materialize_archive_link(destination, member)
    except tarfile.TarError as exc:
        raise BootstrapError(f"Unsupported or corrupt dependency archive: {archive}") from exc


def _materialize_archive_link(destination: Path, member: "tarfile.TarInfo") -> None:
    """Creates a validated internal link, copying its target when links are unsupported.

    A Windows host without the symbolic-link privilege raises OSError from ``os.symlink``.
    The documented path must stay readable either way, so the validated in-tree target's
    content is copied in place of the link.
    """
    link_path = destination / member.name
    link_path.parent.mkdir(parents=True, exist_ok=True)
    if link_path.is_symlink() or link_path.is_file():
        link_path.unlink()
    if member.issym():
        # A symbolic-link target is relative to the link's own directory. Windows stores the
        # target text literally and resolves only backslash separators, so the POSIX spelling
        # from the archive is converted; a link that still does not resolve is replaced by a
        # copy rather than left as an unreadable reparse point.
        target_path = link_path.parent / posixpath.normpath(member.linkname)
        try:
            os.symlink(
                member.linkname.replace("/", os.sep),
                link_path,
                target_is_directory=target_path.is_dir(),
            )
            if link_path.exists():
                return
            link_path.unlink()
        except OSError:
            pass
    else:
        # A hard-link target is named from the archive root.
        target_path = destination / posixpath.normpath(member.linkname)
        try:
            os.link(target_path, link_path)
            return
        except OSError:
            pass
    if target_path.is_dir():
        shutil.copytree(target_path, link_path)
    elif target_path.is_file():
        shutil.copy2(target_path, link_path)
    else:
        raise BootstrapError(
            f"Archive link target is missing after extraction: {member.name} -> {member.linkname}"
        )


def _tls_context() -> ssl.SSLContext:
    """A default TLS context minus ``VERIFY_X509_STRICT``.

    Python 3.13 turned on strict RFC 5280 checks by default, which reject the
    re-signed chains of corporate TLS-inspection proxies whose CA certificate
    does not mark Basic Constraints as critical. Trust-chain and hostname
    verification stay enabled, and every downloaded archive is still validated
    against its pinned checksum.
    """
    context = ssl.create_default_context()
    context.verify_flags &= ~ssl.VERIFY_X509_STRICT
    return context


def _urlopen(url: str):
    return urllib.request.urlopen(url, context=_tls_context())


def _download_archive(package: Dict, cache_dir: Path) -> Path:
    archive = cache_dir / package["archive_name"]
    cache_dir.mkdir(parents=True, exist_ok=True)
    temporary = archive.with_suffix(archive.suffix + ".download.tmp")
    temporary.unlink(missing_ok=True)
    _message(f"Download {package['name']} {package['version']} -> {archive}")
    try:
        with _urlopen(package["url"]) as response, temporary.open("wb") as output:
            shutil.copyfileobj(response, output)
        _check_digests(package, temporary, "downloaded archive")
        temporary.replace(archive)
    finally:
        temporary.unlink(missing_ok=True)
    return archive


def _download_signature(package: Dict, cache_dir: Path) -> Optional[Path]:
    signature_path = _signature_path(package, cache_dir)
    if signature_path is None or signature_path.is_file():
        return signature_path
    signature_url = package["signature"].get("url")
    if not isinstance(signature_url, str) or not signature_url:
        raise BootstrapError(f"Release signature URL is missing for {package['name']}.")

    temporary = signature_path.with_suffix(signature_path.suffix + ".download.tmp")
    temporary.unlink(missing_ok=True)
    _message(f"Download {package['name']} release signature -> {signature_path}")
    try:
        with _urlopen(signature_url) as response, temporary.open("wb") as output:
            shutil.copyfileobj(response, output)
        temporary.replace(signature_path)
    finally:
        temporary.unlink(missing_ok=True)
    _verify_signature_cache(package, cache_dir)
    return signature_path


def _prepare_supplemental_files(package: Dict, cache_dir: Path, offline: bool) -> List[Path]:
    cache_dir.mkdir(parents=True, exist_ok=True)
    for item in package.get("supplemental_files", []):
        cached = _supplemental_cache_path(package, item, cache_dir)
        if cached.is_file():
            continue
        if offline:
            _verify_supplemental_files(package, cache_dir)
        url = item.get("url")
        if not isinstance(url, str) or not url:
            raise BootstrapError(
                f"Supplemental file URL is missing for {package['name']}: {cached.name}."
            )
        temporary = cached.with_suffix(cached.suffix + ".download.tmp")
        temporary.unlink(missing_ok=True)
        _message(f"Download {package['name']} supplemental file -> {cached}")
        try:
            with _urlopen(url) as response, temporary.open("wb") as output:
                shutil.copyfileobj(response, output)
            temporary.replace(cached)
        finally:
            temporary.unlink(missing_ok=True)
    return _verify_supplemental_files(package, cache_dir)


def _prepare_git_source(package: Dict, cache_dir: Path, offline: bool) -> Path:
    source = package["source"]
    checkout = cache_dir / source["checkout_dir"]
    if _git_checkout_valid(package, checkout):
        return checkout

    if checkout.exists():
        actual = _git_head(checkout) or "not a Git checkout"
        raise BootstrapError(
            f"Cached checkout has the wrong identity for {package['name']}: {checkout}; "
            f"expected {source['commit']}, actual {actual}. Remove only this cache directory and retry."
        )

    git = shutil.which("git")
    if not git:
        raise BootstrapError(f"Git is required to prepare {package['name']}.")

    cache_dir.mkdir(parents=True, exist_ok=True)
    if offline:
        cache_item, _ = _verify_git_cache(package, cache_dir)
        if cache_item.suffix.lower() != ".bundle":
            return cache_item
        completed = _run([git, "clone", "--no-checkout", str(cache_item), str(checkout)])
    else:
        completed = _run([git, "clone", "--no-checkout", source["url"], str(checkout)])
    if completed.returncode != 0:
        raise BootstrapError(f"Git clone failed for {package['name']}.")

    completed = _run([git, "-C", str(checkout), "checkout", "--detach", source["commit"]])
    if completed.returncode != 0 or not _git_checkout_valid(package, checkout):
        raise BootstrapError(
            f"Cannot check out pinned commit {source['commit']} for {package['name']}: {checkout}"
        )
    return checkout


def _prepare_archive_source(package: Dict, cache_dir: Path, offline: bool) -> Tuple[Path, Path]:
    archive = cache_dir / package["archive_name"]
    if archive.exists():
        _verify_archive_digest(package, archive)
    elif offline:
        _verify_archive(package, archive, cache_dir)
    else:
        archive = _download_archive(package, cache_dir)

    if offline:
        _verify_signature_cache(package, cache_dir)
    else:
        _download_signature(package, cache_dir)

    extract_root = cache_dir / package["config"]["extracted_dir"]
    working_dir = extract_root / package["config"].get("build_working_dir", ".")
    if not working_dir.exists():
        if extract_root.exists() and any(extract_root.iterdir()):
            raise BootstrapError(
                f"Cached extraction is incomplete for {package['name']}: {extract_root}. "
                "Remove only this extraction directory and retry."
            )
        _message(f"Extract {archive} -> {extract_root}")
        _safe_extract_archive(archive, extract_root)
    if not working_dir.exists():
        raise BootstrapError(f"Expected source directory was not found after extraction: {working_dir}")
    return extract_root, working_dir


def _copy_source(root: Path, relative: str) -> Path:
    source = (root / relative).resolve()
    try:
        source.relative_to(root.resolve())
    except ValueError as exc:
        raise BootstrapError(f"Copy rule escapes the dependency cache: {relative}") from exc
    return source


def _copy_rules_ready(package: Dict, source_root: Path) -> bool:
    return all(_copy_source(source_root, rule["from"]).exists() for rule in package["config"].get("copy_rules", []))


def _build_commands(package: Dict) -> List:
    return package.get("config", {}).get("build", {}).get("commands", [])


def _build_recipe(package: Dict) -> Dict:
    """Identify the recipe that produces the cached build outputs."""
    config = package.get("config", {})
    return {
        "schema_version": 1,
        "name": package["name"],
        "version": package.get("version", ""),
        "platform": package.get("resolved_platform", _host_platform()),
        "architecture": package.get("resolved_architecture", _host_architecture()),
        "extracted_dir": config.get("extracted_dir", ""),
        "build_working_dir": config.get("build_working_dir", "."),
        "build": config.get("build", {}),
    }


def _build_stamp_path(package: Dict, cache_dir: Path) -> Path:
    recipe = _build_recipe(package)
    key = "-".join((recipe["name"], recipe["platform"], recipe["architecture"]))
    return cache_dir / f".wse-build-{key}.json"


def _cached_build_current(package: Dict, cache_dir: Path) -> bool:
    """Whether the cached build outputs came from the current recipe.

    Build outputs live in the extraction cache and outlive any single run, so a
    changed build command would otherwise be ignored for as long as those
    outputs remain. Packages that declare no build commands have nothing to
    invalidate.
    """
    if not _build_commands(package):
        return True
    stamp = _build_stamp_path(package, cache_dir)
    if not stamp.is_file():
        return False
    try:
        recorded = json.loads(stamp.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    return recorded == _build_recipe(package)


def _write_build_stamp(package: Dict, cache_dir: Path) -> None:
    if not _build_commands(package):
        return
    stamp = _build_stamp_path(package, cache_dir)
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text(
        json.dumps(_build_recipe(package), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def _run_build(
    package: Dict,
    source_root: Path,
    working_dir: Path,
    vendor_root: Path,
    cmake: Optional[str],
) -> None:
    commands = package["config"].get("build", {}).get("commands", [])
    if not commands:
        raise BootstrapError(f"No build commands are defined for {package['name']}.")
    cmake_executable = cmake or os.environ.get("WSE_CMAKE") or shutil.which("cmake")
    if not cmake_executable:
        raise BootstrapError("CMake was not found. Set WSE_CMAKE or pass --cmake <path>.")

    # Keep generated build paths short on Windows; deeply nested source/cache paths can exceed MSBuild limits.
    with tempfile.TemporaryDirectory(prefix=f"wse-build-{package['name']}-") as temporary:
        replacements = {
            "{extract_root}": str(source_root),
            "{build_root}": temporary,
            "{vendor_root}": str(vendor_root),
            "{wse_root}": str(WSE_ROOT),
        }
        for raw_command in commands:
            command: List[str] = []
            for index, raw_arg in enumerate(raw_command):
                value = str(raw_arg)
                for token, replacement in replacements.items():
                    value = value.replace(token, replacement)
                if index == 0 and value.lower() == "cmake":
                    value = cmake_executable
                command.append(value)
            completed = _run(command, cwd=working_dir)
            if completed.returncode != 0:
                raise BootstrapError(f"Build command failed for {package['name']}: {' '.join(command)}")


def _copy_rules(package: Dict, source_root: Path, staging: Path) -> None:
    for rule in package["config"].get("copy_rules", []):
        source = _copy_source(source_root, rule["from"])
        destination = staging / rule.get("to", "")
        if not source.exists():
            raise BootstrapError(f"Build output is missing for {package['name']}: {source}")
        if rule.get("type", "dir") == "file":
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        else:
            shutil.copytree(source, destination, dirs_exist_ok=True)


def _copy_supplemental_files(package: Dict, cache_dir: Path, staging: Path) -> None:
    for item in package.get("supplemental_files", []):
        relative_target = item.get("target")
        if (
            not isinstance(relative_target, str)
            or not relative_target
            or Path(relative_target).is_absolute()
            or ".." in Path(relative_target).parts
        ):
            raise BootstrapError(
                f"Invalid supplemental target for {package['name']}: {relative_target!r}"
            )
        source = _supplemental_cache_path(package, item, cache_dir)
        destination = staging / relative_target
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def _write_dependency_metadata(package: Dict, staging: Path) -> None:
    metadata_file = package.get("metadata_file")
    if not metadata_file:
        return
    destination = staging / metadata_file
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(
        json.dumps(_dependency_metadata(package), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def provision_package(
    package: Dict,
    cache_dir: Path,
    vendor_root: Path,
    offline: bool = False,
    force: bool = False,
    cmake: Optional[str] = None,
    target_architecture: Optional[str] = None,
) -> str:
    package = _resolve_platform(package, target_architecture)
    target = _target_for(package, vendor_root)
    missing = _required_missing(package, target)
    if target.exists() and not missing:
        return f"ready -> {target}"
    if target.exists() and missing and not force:
        # A sole identity mismatch means the recipe changed, not that a write was interrupted.
        outdated = all(item.endswith("(identity mismatch)") for item in missing)
        reason = (
            "was produced by a different recipe" if outdated else "is partial"
        )
        raise BootstrapError(
            f"Dependency target {reason} for {package['name']}: {target}; missing: {', '.join(missing)}. "
            "Use --force only to replace this bootstrap-managed target."
        )

    _prepare_supplemental_files(package, cache_dir, offline)

    source = package.get("source", {})
    if source.get("type") == "git":
        source_root = _prepare_git_source(package, cache_dir, offline)
        working_dir = source_root / package["config"].get("build_working_dir", ".")
    else:
        source_root, working_dir = _prepare_archive_source(package, cache_dir, offline)

    stale_recipe = not _cached_build_current(package, cache_dir)
    if stale_recipe and _copy_rules_ready(package, source_root):
        _message(f"Rebuild {package['name']}: the build recipe changed since the cached outputs.")
    if stale_recipe or not _copy_rules_ready(package, source_root):
        _run_build(package, source_root, working_dir, vendor_root, cmake)
        _write_build_stamp(package, cache_dir)
    if not _copy_rules_ready(package, source_root):
        raise BootstrapError(f"Build completed without all declared outputs for {package['name']}.")

    # Keep staging beside the final target. On Windows, moving a directory
    # across parent directories can preserve the source ACL instead of
    # inheriting the vendor root ACL, leaving the installed target unreadable
    # to later processes.
    target.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(
        tempfile.mkdtemp(prefix=f".wse-{package['name']}-", dir=str(target.parent))
    )
    try:
        _copy_rules(package, source_root, staging)
        _copy_supplemental_files(package, cache_dir, staging)
        _write_dependency_metadata(package, staging)
        staged_missing = _required_missing(package, staging)
        if staged_missing:
            raise BootstrapError(
                f"Staged dependency is incomplete for {package['name']}; missing: {', '.join(staged_missing)}"
            )
        if target.exists():
            shutil.rmtree(target)
        staging.replace(target)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
    return f"installed -> {target}"


def _selected_packages(manifest: Dict, names: Iterable[str]) -> List[Dict]:
    requested = set(names)
    packages = [package for package in manifest["packages"] if package.get("role") == "dependency"]
    known = {package["name"] for package in packages}
    unknown = sorted(requested - known)
    if unknown:
        raise BootstrapError(f"Unknown package name(s): {', '.join(unknown)}")
    if requested:
        return [package for package in packages if package["name"] in requested]
    return [package for package in packages if package.get("default", True)]


def provision_toolchain(
    toolchain: Dict,
    cache_dir: Path,
    *,
    offline: bool,
    allow_fetch: bool,
) -> Tuple[Path, Optional[Path], str]:
    """Resolves one verification tool, preferring what is already installed on this machine.

    A fetched toolchain lands in the gitignored bootstrap cache, never in ``vendor/`` and never in
    an install package, so provisioning it does not change what WSE redistributes.
    """
    name = toolchain["name"]
    detect_spec = toolchain.get("detect", {})

    detected = detect_toolchain(toolchain)
    if detected is not None:
        root, executable, version = detected
        printable = ".".join(str(part) for part in version) if version else "unknown version"
        return root, executable, f"detected {printable} -> {root}"

    cached_root = _toolchain_root(toolchain, cache_dir)
    if cached_root.is_dir() and not _toolchain_missing_files(toolchain, cached_root):
        executable = _executable_for_root(cached_root, detect_spec)
        return cached_root, executable, f"cached -> {cached_root}"

    hint = toolchain.get("install_hint", "") or f"Install {name} and re-run bootstrap."
    requirement = _toolchain_requirement(toolchain)
    source = _toolchain_host_source(toolchain)
    if source is None:
        raise BootstrapError(
            f"No {name} {requirement} was found and this host has no pinned archive. " + hint
        )
    if not allow_fetch:
        raise BootstrapError(
            f"No {name} {requirement} was found and --no-fetch-toolchain forbids fetching. " + hint
        )

    archive_spec = {
        "name": name,
        "version": toolchain["version"],
        "url": source["url"],
        "archive_name": source["archive_name"],
    }
    archive_spec.update(
        {algorithm: value for algorithm, value in source.items() if algorithm in DIGEST_ALGORITHMS}
    )
    # Fail before any download when the manifest pins no digest for this host.
    _pinned_digests(archive_spec)
    archive = cache_dir / source["archive_name"]
    if offline:
        _verify_archive_digest(archive_spec, archive)
    else:
        archive = _download_archive(archive_spec, cache_dir)
        _verify_archive_digest(archive_spec, archive)

    staging = cache_dir / TOOLCHAIN_STAGING / name
    if staging.exists():
        _remove_tree(staging)
    # An upstream toolchain legitimately contains internal relative links.
    _safe_extract_archive(archive, staging, allow_internal_links=True)

    extracted = staging / source["extracted_root"] if source.get("extracted_root") else staging
    if not extracted.is_dir():
        raise BootstrapError(
            f"The {name} archive did not contain the pinned root '{source.get('extracted_root')}'."
        )

    cached_root.parent.mkdir(parents=True, exist_ok=True)
    if cached_root.exists():
        _remove_tree(cached_root)
    # A rename within the cache keeps the deep trees intact; only the top directory is touched.
    os.replace(extracted, cached_root)
    shutil.rmtree(_extended_length_path(staging), ignore_errors=True)

    missing = _toolchain_missing_files(toolchain, cached_root)
    if missing:
        raise BootstrapError(
            f"The provisioned {name} is incomplete: {cached_root}; missing {', '.join(missing)}."
        )
    executable = _executable_for_root(cached_root, detect_spec)
    return cached_root, executable, f"fetched {toolchain['version']} -> {cached_root}"


def _default_base_preset(target_architecture: Optional[str] = None) -> str:
    """The official preset the generated one inherits.

    A cross build has to inherit the preset that carries the cross toolchain file, and a native
    build must not, so the base follows the target architecture rather than the host alone.
    """
    if _host_platform() != "windows":
        if _normalize_architecture(target_architecture) != _host_architecture():
            return "linux-arm64-gcc-shared-core"
        return "linux-gcc-shared-core"
    return "windows-msvc-shared-core"


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--cache-dir", type=Path, default=DEFAULT_CACHE)
    parser.add_argument("--vendor-root", type=Path, default=DEFAULT_VENDOR)
    parser.add_argument("--offline", action="store_true", help="Forbid every remote source operation")
    parser.add_argument("--verify-cache", action="store_true", help="Validate the portable cache without installing")
    parser.add_argument("--check", action="store_true", help="Validate installed dependency targets without changing them")
    parser.add_argument("--force", action="store_true", help="Replace incomplete bootstrap-managed targets")
    parser.add_argument("--skip-existing", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--cmake", help="CMake executable used when cached build outputs are absent")
    parser.add_argument(
        "--target-architecture",
        choices=("x86_64", "arm64"),
        help="Provision artifacts for this architecture instead of the current host",
    )
    parser.add_argument("--package", action="append", default=[], help="Prepare only the named package (repeatable)")
    parser.add_argument(
        "--language",
        action="append",
        default=[],
        help="Prepare everything one language binding needs, including its toolchain (repeatable)",
    )
    parser.add_argument(
        "--component",
        action="append",
        default=[],
        help="Prepare everything one component needs and enable it in the preset (repeatable)",
    )
    parser.add_argument(
        "--no-fetch-toolchain",
        action="store_true",
        help="Never download a toolchain; fail when none is already installed",
    )
    parser.add_argument(
        "--user-presets",
        type=Path,
        default=DEFAULT_USER_PRESETS,
        help="Path of the generated CMakeUserPresets.json",
    )
    parser.add_argument("--preset-name", default="wse-local", help="Name of the generated configure preset")
    parser.add_argument(
        "--extension-root",
        type=Path,
        default=None,
        help="Root of an access-controlled extension overlay that supplies components this "
        "repository does not carry",
    )
    parser.add_argument(
        "--base-preset",
        default="",
        help="Official preset the generated preset inherits; defaults to the host core preset",
    )
    parser.add_argument(
        "--no-write-presets",
        action="store_true",
        help="Resolve and report the selection without writing CMakeUserPresets.json",
    )
    args = parser.parse_args(argv)

    try:
        manifest = _load_manifest(args.manifest.resolve())
        cache_dir = args.cache_dir.resolve()
        vendor_root = args.vendor_root.resolve()

        selection_packages: List[str] = []
        toolchain_names: List[str] = []
        preset_variables: Dict[str, str] = {}
        if args.language or args.component:
            selection_packages, toolchain_names, preset_variables = _resolve_selection(
                manifest, args.language, args.component, _selection_target(args.target_architecture)
            )
            _message(
                "Selection: "
                + f"languages=[{', '.join(args.language) or '-'}] "
                + f"components=[{', '.join(args.component) or '-'}]"
            )
        if args.package:
            packages = _selected_packages(manifest, args.package)
        elif args.language or args.component:
            # An explicit selection is authoritative: a language that needs no vendored dependency
            # must not silently pull the default package set.
            packages = _selected_packages(manifest, selection_packages) if selection_packages else []
        else:
            packages = _selected_packages(manifest, [])

        if args.verify_cache:
            for package in packages:
                _message(f"cache OK: {package['name']} {package['version']} -> {verify_cache(package, cache_dir)}")
        if args.check:
            failures = []
            for package in packages:
                package = _resolve_platform(package, args.target_architecture)
                target = _target_for(package, vendor_root)
                missing = _required_missing(package, target)
                if missing:
                    failures.append(f"{package['name']} {package['version']}: {target}; missing {', '.join(missing)}")
                else:
                    _message(f"target OK: {package['name']} {package['version']} -> {target}")
            if failures:
                raise BootstrapError("Dependency check failed:\n  - " + "\n  - ".join(failures))
        if not args.verify_cache and not args.check:
            if args.skip_existing:
                _message("--skip-existing is deprecated; existing targets are reused only after full validation.")
            for package in packages:
                result = provision_package(
                    package,
                    cache_dir,
                    vendor_root,
                    offline=args.offline,
                    force=args.force,
                    cmake=args.cmake,
                    target_architecture=args.target_architecture,
                )
                _message(f"{package['name']} {package['version']}: {result}")

        if (args.language or args.component) and not args.verify_cache:
            cache_variables = dict(preset_variables)
            if vendor_root != DEFAULT_VENDOR.resolve():
                # CMake defaults the dependency root to `<source>/vendor`, so a preset that omitted
                # a relocated `--vendor-root` would configure against dependencies that are not
                # there.
                cache_variables["WSE_DEPENDENCY_ROOT"] = vendor_root.as_posix()
            if args.extension_root is not None:
                cache_variables["WSE_EXTENSION_ROOT"] = (
                    args.extension_root.resolve().as_posix()
                )
            # Some components are implemented in an overlay outside this repository. Writing a
            # preset that selects one without naming the overlay produces a preset that cannot
            # configure, so say so here rather than at the user's next command.
            overlay_only = sorted(
                name
                for name, option in (("vpj", "WSE_BUILD_VPJ"), ("vpj-video", "WSE_BUILD_VPJ_VIDEO"))
                if cache_variables.get(option) == "ON" and args.extension_root is None
            )
            if overlay_only:
                raise BootstrapError(
                    "These components are supplied by an extension overlay: "
                    + ", ".join(overlay_only)
                    + ". Pass --extension-root with the overlay path, or leave them out."
                )
            for package in packages:
                cache_variables.update(
                    _package_cmake_values(package, vendor_root, args.target_architecture)
                )
            for toolchain in _selected_toolchains(manifest, toolchain_names):
                if args.check:
                    detected = detect_toolchain(toolchain)
                    state = "installed" if detected else "missing"
                    _message(f"toolchain {toolchain['name']}: {state}")
                    continue
                root, executable, result = provision_toolchain(
                    toolchain,
                    cache_dir,
                    offline=args.offline,
                    allow_fetch=not args.no_fetch_toolchain,
                )
                _message(f"toolchain {toolchain['name']}: {result}")
                cache_variables.update(_toolchain_cmake_values(toolchain, root, executable))

            if not args.check and not args.no_write_presets:
                base_preset = args.base_preset or _default_base_preset(args.target_architecture)
                written = _write_user_presets(
                    args.user_presets.resolve(),
                    args.preset_name,
                    base_preset,
                    cache_variables,
                    "Generated by bootstrap.py; edit the selection and re-run instead of this file.",
                )
                _message(f"Wrote {written}")
                _message(f"Configure with: cmake --preset {args.preset_name}")
        return 0
    except BootstrapError as exc:
        _message(f"ERROR: {exc}")
        if args.offline:
            _message("Offline mode did not attempt a remote fallback.")
        return 2


if __name__ == "__main__":
    sys.exit(main())
