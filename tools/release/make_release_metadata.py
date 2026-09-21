#!/usr/bin/env python3
"""Generates the release metadata for an installed WSE package.

Reads the package's ``wse-package.json`` and the ``wse-dependency.json`` identity files the
package's ``vendor/`` tree carries, and writes two files into the package root:

* ``sbom.cdx.json`` - a CycloneDX 1.5 software bill of materials naming WSE itself and every
  vendored dependency with its version, license, and source hash.
* ``THIRD_PARTY_NOTICES.md`` - the human-readable notice list pointing at each dependency's
  license text inside the package.

The script is offline and uses only the standard library, like bootstrap.py.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _component(name: str, version: str, license_id: str, description: str, hashes: list) -> dict:
    component = {
        "type": "library",
        "name": name,
        "version": version,
        "description": description,
        "licenses": [{"license": {"name": license_id}}],
    }
    if hashes:
        component["hashes"] = hashes
    return component


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-root", type=Path, required=True,
                        help="Root of an installed WSE package")
    arguments = parser.parse_args()
    package_root = arguments.package_root.resolve()

    manifest_path = package_root / "wse-package.json"
    if not manifest_path.is_file():
        print(f"Missing package manifest: {manifest_path}", file=sys.stderr)
        return 1
    manifest = _load_json(manifest_path)
    version = manifest.get("version", "unknown")

    license_expression = manifest.get("license")
    license_files = manifest.get("license_files", [])
    if (not isinstance(license_expression, str) or not license_expression
            or not isinstance(license_files, list)
            or not all(isinstance(name, str) for name in license_files)
            or not {"LICENSE", "NOTICE"}.issubset(license_files)):
        print("Missing explicit package license metadata.", file=sys.stderr)
        return 1
    if any(not isinstance(name, str) or Path(name).name != name
           or not (package_root / name).is_file() for name in license_files):
        print("Missing or invalid package license file.", file=sys.stderr)
        return 1
    has_extension = manifest.get("extension_commit") not in (None, "", "none")
    if has_extension and (license_expression == "Apache-2.0" or "LICENSE.extension" not in license_files):
        print("An extension package must retain its separate license terms.", file=sys.stderr)
        return 1

    components = [_component(
        "wonderstew-engine", version, license_expression,
        f"WSE {manifest.get('library_type', '?')} package, source tree {manifest.get('source_commit', '?')}",
        [],
    )]
    components[0]["licenses"] = [{"expression": license_expression}]
    notices = [
        "# Third-Party Notices",
        "",
        f"WonderStewEngine {version} builds on the third-party components below. Each keeps its",
        "own license; the license texts live at the recorded paths inside this package.",
        "",
    ]

    # A dependency the package actually carries has its identity file in vendor/. The manifest's
    # dependency catalog lists every pinned dependency whether shipped or not, so the vendor tree
    # decides what enters the SBOM and the notices.
    vendor_root = package_root / "vendor"
    shipped = sorted(vendor_root.glob("*/wse-dependency.json")) if vendor_root.is_dir() else []
    for identity_path in shipped:
        identity = _load_json(identity_path)
        name = identity.get("name", identity_path.parent.name)
        source = identity.get("source", {})
        hashes = []
        if source.get("sha256"):
            hashes.append({"alg": "SHA-256", "content": source["sha256"]})
        catalog_entry = manifest.get("dependency_catalog", {}).get(name, {})
        dependency_version = catalog_entry.get("version") or source.get("archive_name", "unknown")
        components.append(_component(
            name, dependency_version, identity.get("license", "unknown"),
            catalog_entry.get("integration_status", "vendored dependency"), hashes))
        license_file = identity.get("license_file")
        license_note = (
            f"`vendor/{identity_path.parent.name}/{license_file}`" if license_file
            else "its upstream distribution")
        notices.append(
            f"- **{name}** {dependency_version} - license: {identity.get('license', 'unknown')}"
            f" - text: {license_note}")

    if not shipped:
        notices.append("- This package variant vendors no third-party component.")

    sbom = {
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "version": 1,
        "metadata": {
            "timestamp": manifest.get("generated_utc", ""),
            "component": components[0],
        },
        "components": components[1:],
    }
    (package_root / "sbom.cdx.json").write_text(
        json.dumps(sbom, indent=2) + "\n", encoding="utf-8")
    (package_root / "THIRD_PARTY_NOTICES.md").write_text(
        "\n".join(notices) + "\n", encoding="utf-8")
    print(f"release metadata written: {package_root / 'sbom.cdx.json'} "
          f"({len(components) - 1} third-party components)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
