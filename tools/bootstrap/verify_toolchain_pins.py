#!/usr/bin/env python3
"""Verify every pinned verification-toolchain archive, including hosts this machine cannot run.

The manifest pins toolchain archives for Windows and Linux on x86-64 and ARM64, but only the host
running the gates ever executes one. This tool closes the rest of the gap without those hosts:

  identity  Re-reads the digest the vendor publishes today and compares it with the manifest, so a
            transcription error or a re-cut upstream release is caught rather than assumed.
  contents  Downloads each archive, verifies every digest the manifest pins, and checks from the
            archive's own member table that `extracted_root` and every `required_files` entry are
            there. That is where the per-platform mistakes live: a JDK names `bin/javac.exe` on
            Windows and `bin/javac` on Linux, and a misspelled root directory fails only at
            provisioning time. Reading the member table rather than an extracted tree keeps a
            foreign-platform archive checked exactly as strictly as a native one.
  extraction Runs bootstrap's real extraction as well, so a member bootstrap would refuse is caught
            here rather than on the host that finally provisions it. An extraction that the host
            itself cannot perform - materializing a foreign symbolic link, for instance - is
            reported rather than counted as a failure of the archive.

What it cannot show is that a foreign-architecture binary runs. Executing a Linux or ARM64 toolchain
still requires that host.

This tool needs the network, so it is not part of the default offline gates. Run it when a pinned
version changes, and before trusting a host that has never been provisioned.
"""

from __future__ import annotations

import argparse
import base64
import gzip
import importlib.util
import json
import posixpath
import sys
import tarfile
import tempfile
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path
from typing import Dict, List, Optional, Sequence


WSE_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = WSE_ROOT / "bootstrap-manifest.json"
USER_AGENT = "wse-verify-toolchain-pins/1.0"

_SPEC = importlib.util.spec_from_file_location("wse_bootstrap", WSE_ROOT / "bootstrap.py")
BOOTSTRAP = importlib.util.module_from_spec(_SPEC)
assert _SPEC.loader is not None
_SPEC.loader.exec_module(BOOTSTRAP)


class VerificationError(RuntimeError):
    """A pinned archive does not match what its vendor publishes, or does not contain what it must."""


def _get(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=180) as response:
        payload = response.read()
    # The NuGet registration endpoints answer with gzip regardless of the request headers.
    return gzip.decompress(payload) if payload[:2] == b"\x1f\x8b" else payload


def _get_json(url: str) -> Dict:
    return json.loads(_get(url).decode("utf-8"))


# ----------------------------------------------------------------------------------------------
# Vendor-published digests
#
# Each resolver returns {algorithm: digest} straight from the publisher, never from the manifest.
# The knowledge of where a vendor publishes its digest belongs here rather than in the manifest,
# which stays a plain record of what is pinned.
# ----------------------------------------------------------------------------------------------


def _nodejs_published(toolchain: Dict, _platform: str, _architecture: str, source: Dict) -> Dict:
    text = _get(f"https://nodejs.org/dist/v{toolchain['version']}/SHASUMS256.txt").decode("utf-8")
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1] == source["archive_name"]:
            return {"sha256": parts[0].lower()}
    raise VerificationError(f"{source['archive_name']} is not listed in the Node.js SHASUMS256.txt")


def _temurin_published(toolchain: Dict, platform_name: str, architecture: str, source: Dict) -> Dict:
    release = urllib.parse.quote(f"jdk-{toolchain['version']}", safe="")
    data = _get_json(
        f"https://api.adoptium.net/v3/assets/release_name/eclipse/{release}"
        "?image_type=jdk&project=jdk"
    )
    wanted = {"x86_64": "x64", "arm64": "aarch64"}[architecture]
    for binary in data["binaries"]:
        if binary["os"] == platform_name and binary["architecture"] == wanted:
            return {"sha256": binary["package"]["checksum"].lower()}
    raise VerificationError(
        f"Adoptium does not publish a {platform_name}/{wanted} JDK for {toolchain['version']}"
    )


def _dotnet_published(toolchain: Dict, platform_name: str, architecture: str, source: Dict) -> Dict:
    channel = ".".join(toolchain["version"].split(".")[:2])
    data = _get_json(
        f"https://builds.dotnet.microsoft.com/dotnet/release-metadata/{channel}/releases.json"
    )
    label = {"windows": "win", "linux": "linux"}[platform_name]
    suffix = {"x86_64": "x64", "arm64": "arm64"}[architecture]
    for release in data["releases"]:
        sdk = release.get("sdk", {})
        if sdk.get("version") != toolchain["version"]:
            continue
        for entry in sdk["files"]:
            if entry["url"] == source["url"]:
                return {"sha512": entry["hash"].lower()}
        raise VerificationError(
            f"The .NET {toolchain['version']} release does not publish {label}-{suffix}"
        )
    raise VerificationError(f"The .NET {channel} channel no longer lists SDK {toolchain['version']}")


def _cpython_published(toolchain: Dict, _platform: str, architecture: str, source: Dict) -> Dict:
    package = {"x86_64": "python", "arm64": "pythonarm64"}[architecture]
    version = toolchain["version"]
    registration = _get_json(
        f"https://api.nuget.org/v3/registration5-gz-semver2/{package}/{version}.json"
    )
    catalog = _get_json(registration["catalogEntry"])
    if catalog["packageHashAlgorithm"] != "SHA512":
        raise VerificationError(f"NuGet published an unexpected hash algorithm for {package}")
    return {"sha512": base64.b64decode(catalog["packageHash"]).hex()}


PUBLISHERS = {
    "nodejs": _nodejs_published,
    "temurin-jdk": _temurin_published,
    "dotnet-sdk": _dotnet_published,
    "cpython": _cpython_published,
}


def verify_identity(toolchain: Dict, platform_name: str, architecture: str, source: Dict) -> str:
    """Compares the manifest against what the vendor publishes right now."""
    publisher = PUBLISHERS.get(toolchain["name"])
    if publisher is None:
        raise VerificationError(f"No published-digest resolver for {toolchain['name']}")
    published = publisher(toolchain, platform_name, architecture, source)
    pinned = dict(BOOTSTRAP._pinned_digests(dict(source, name=toolchain["name"], version=toolchain["version"])))

    shared = set(pinned) & set(published)
    if not shared:
        raise VerificationError(
            f"The manifest pins {sorted(pinned)} but the vendor publishes {sorted(published)}"
        )
    for algorithm in sorted(shared):
        if pinned[algorithm] != published[algorithm]:
            raise VerificationError(
                f"{BOOTSTRAP.DIGEST_LABELS[algorithm]} differs: manifest {pinned[algorithm]}, "
                f"vendor {published[algorithm]}"
            )
    return ", ".join(BOOTSTRAP.DIGEST_LABELS[algorithm] for algorithm in sorted(shared))


def _archive_entries(archive: Path) -> Dict[str, str]:
    """Maps every member name to `dir`, `file`, or `sym:<target>`.

    The layout is checked against this table rather than against an extracted tree, so an archive
    for a foreign platform is checked exactly as strictly as a native one. Materializing a Linux
    symbolic link on Windows is a property of the host, not of the archive.
    """
    entries: Dict[str, str] = {}
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                name = posixpath.normpath(member.filename)
                entries[name] = "dir" if member.is_dir() else "file"
    else:
        with tarfile.open(archive, mode="r:*") as source:
            for member in source:
                name = posixpath.normpath(member.name)
                if member.isdir():
                    entries[name] = "dir"
                elif member.issym():
                    entries[name] = f"sym:{member.linkname}"
                elif member.islnk():
                    entries[name] = f"sym:/{posixpath.normpath(member.linkname)}"
                else:
                    entries[name] = "file"

    # A NuGet package stores only file entries, so `tools/` never appears on its own. Filling in the
    # implied parents keeps the layout check independent of how a producer wrote the archive.
    for name in list(entries):
        parent = posixpath.dirname(name)
        while parent and parent != "." and parent not in entries:
            entries[parent] = "dir"
            parent = posixpath.dirname(parent)
    return entries


def _resolve_entry(entries: Dict[str, str], name: str) -> str:
    """Follows link members inside the archive so a required file may legitimately be a link."""
    for _ in range(8):
        kind = entries.get(name)
        if kind is None or not kind.startswith("sym:"):
            return kind or "missing"
        target = kind[4:]
        # A hard link was recorded archive-root relative and carries a leading slash.
        name = (
            posixpath.normpath(target[1:])
            if target.startswith("/")
            else posixpath.normpath(posixpath.join(posixpath.dirname(name), target))
        )
    raise VerificationError(f"Link chain too deep for {name}")


def verify_contents(toolchain: Dict, source: Dict, cache_dir: Path, keep: bool) -> str:
    """Downloads and digests one archive, then checks the layout the manifest claims it has."""
    spec = {
        "name": toolchain["name"],
        "version": toolchain["version"],
        "url": source["url"],
        "archive_name": source["archive_name"],
    }
    spec.update(
        {algorithm: value for algorithm, value in source.items() if algorithm in BOOTSTRAP.DIGEST_ALGORITHMS}
    )

    cache_dir.mkdir(parents=True, exist_ok=True)
    archive = cache_dir / source["archive_name"]
    if archive.is_file():
        BOOTSTRAP._check_digests(spec, archive, str(archive))
    else:
        archive = BOOTSTRAP._download_archive(spec, cache_dir)

    try:
        entries = _archive_entries(archive)
        root = source.get("extracted_root")
        if root:
            if entries.get(root) != "dir":
                raise VerificationError(f"The archive does not contain the pinned root '{root}'")
            prefix = root + "/"
        else:
            prefix = ""

        wrong = [
            f"{name} ({_resolve_entry(entries, prefix + name)})"
            for name in source["required_files"]
            if _resolve_entry(entries, prefix + name) != "file"
        ]
        if wrong:
            raise VerificationError(f"Required file(s) not present as files: {', '.join(wrong)}")

        # Run the real extraction too, so a member bootstrap would refuse is caught here rather than
        # on the host that finally provisions it. An OSError is the host declining to materialize a
        # foreign link; a BootstrapError means bootstrap would refuse the archive everywhere.
        staging = Path(tempfile.mkdtemp(prefix=".wse-pin-", dir=str(cache_dir)))
        try:
            BOOTSTRAP._safe_extract_archive(archive, staging, allow_internal_links=True)
            extraction = "extracted"
        except OSError as exc:
            extraction = f"not reproducible on this host ({type(exc).__name__}: {exc})"
        finally:
            # A cleanup failure is not a statement about the archive, so it is reported rather than
            # counted as a verification failure.
            try:
                BOOTSTRAP._remove_tree(staging)
            except OSError as exc:
                print(f"     warning: could not remove {staging}: {exc}", flush=True)

        return f"{len(source['required_files'])} required file(s) present; {extraction}"
    finally:
        if not keep:
            archive.unlink(missing_ok=True)


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument(
        "--cache-dir",
        type=Path,
        default=WSE_ROOT / ".bootstrap-cache" / "pin-verification",
        help="Where archives are downloaded; deleted per archive unless --keep is given",
    )
    parser.add_argument("--toolchain", action="append", default=[], help="Verify only this toolchain")
    parser.add_argument(
        "--identity-only",
        action="store_true",
        help="Compare digests with the vendor without downloading any archive",
    )
    parser.add_argument("--keep", action="store_true", help="Keep downloaded archives in the cache")
    args = parser.parse_args(argv)

    manifest = json.loads(args.manifest.resolve().read_text(encoding="utf-8"))
    toolchains = [
        toolchain
        for toolchain in manifest["toolchains"]
        if not args.toolchain or toolchain["name"] in args.toolchain
    ]

    failures: List[str] = []
    checked = 0
    for toolchain in toolchains:
        for platform_name, entry in sorted(toolchain["platforms"].items()):
            for architecture, source in sorted(entry["architectures"].items()):
                label = f"{toolchain['name']}/{platform_name}/{architecture}"
                checked += 1
                try:
                    identity = verify_identity(toolchain, platform_name, architecture, source)
                    contents = (
                        "skipped"
                        if args.identity_only
                        else verify_contents(toolchain, source, args.cache_dir.resolve(), args.keep)
                    )
                except (VerificationError, BOOTSTRAP.BootstrapError, OSError) as exc:
                    failures.append(f"{label}: {exc}")
                    print(f"FAIL {label}: {exc}", flush=True)
                else:
                    print(f"OK   {label}: vendor {identity}; contents {contents}", flush=True)

    if not args.keep and args.cache_dir.exists() and not any(args.cache_dir.iterdir()):
        args.cache_dir.rmdir()

    print(f"\n{checked - len(failures)}/{checked} pinned archives verified")
    if failures:
        print("Failures:\n  - " + "\n  - ".join(failures))
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
