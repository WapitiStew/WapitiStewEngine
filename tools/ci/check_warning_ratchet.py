#!/usr/bin/env python3
"""The legacy warning ratchet: per-file warning counts may fall, never rise.

Parses a GCC/Clang or MSVC build log, counts warnings per repository-relative source file, and compares
the counts against the reviewed baseline. A file above its baseline fails the gate; a file below
it prints the improved count so the baseline can be lowered in the same change. A file absent
from the baseline is new code and must be warning-clean.

Modes:
  --write-baseline   rewrite the baseline from the log instead of comparing (a reviewed action).
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

MSVC_WARNING = re.compile(r"^\s*(?:\d+>)?(?P<path>[^\n]+?)\(\d+(?:,\d+)?\):\s+warning\s+[A-Z]\d+:", re.M)
WARNING = re.compile(r"^(?P<path>[^\n]+?):\d+(?::\d+)?:\s+warning:", re.M)


def count_warnings(log_text: str, source_root: Path) -> Counter:
    counts: Counter = Counter()
    root = source_root.resolve()
    for match in (match for pattern in (WARNING, MSVC_WARNING)
                  for match in pattern.finditer(log_text)):
        path = Path(match.group("path"))
        if not path.is_absolute():
            path = root / path
        try:
            relative = path.resolve().relative_to(root).as_posix()
        except ValueError:
            continue  # a system or vendor header is not ours to ratchet
        if relative.startswith(("vendor/", ".bootstrap-cache/", "build/")):
            continue
        counts[relative] += 1
    return counts


def load_baseline(path: Path) -> Counter:
    counts: Counter = Counter()
    if not path.is_file():
        return counts
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        name, _, value = line.rpartition("\t")
        counts[name] = int(value)
    return counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True, help="Build log carrying the warnings")
    parser.add_argument("--baseline", type=Path, required=True, help="Reviewed baseline TSV")
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--write-baseline", action="store_true")
    arguments = parser.parse_args()

    counts = count_warnings(arguments.log.read_text(encoding="utf-8", errors="replace"),
                            arguments.source_root)

    if arguments.write_baseline:
        lines = ["# Per-file warning counts under the strict sweep (see WSE_WARNING_SWEEP).",
                 "# A count may only decrease; regenerating this file is a reviewed action.",
                 "# file<TAB>count"]
        lines += [f"{name}\t{count}" for name, count in sorted(counts.items())]
        arguments.baseline.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"baseline written: {sum(counts.values())} warnings across {len(counts)} files")
        return 0

    baseline = load_baseline(arguments.baseline)
    regressions = []
    improvements = []
    for name in sorted(set(counts) | set(baseline)):
        now, was = counts.get(name, 0), baseline.get(name, 0)
        if now > was:
            regressions.append(f"  {name}: {was} -> {now}")
        elif now < was:
            improvements.append(f"  {name}: {was} -> {now}")

    if improvements:
        print("warning counts fell - lower the baseline in this change:")
        print("\n".join(improvements))
    if regressions:
        print("warning ratchet failed - these files gained warnings:", file=sys.stderr)
        print("\n".join(regressions), file=sys.stderr)
        return 1
    print(f"warning ratchet holds: {sum(counts.values())} warnings across {len(counts)} files "
          f"(baseline {sum(baseline.values())})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
