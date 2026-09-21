"""Load installed Linux bindings against their bundled libcamera, without hardware access."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--python")
    parser.add_argument("--node")
    parser.add_argument("--library", action="append", default=[])
    args = parser.parse_args()
    environment = os.environ.copy()
    for name in ("LD_LIBRARY_PATH", "LD_PRELOAD", "PYTHONPATH", "WSE_NODE_ADDON"):
        environment.pop(name, None)

    # A new prefix catches build-tree fallback and stale files from a previous install.
    with tempfile.TemporaryDirectory(prefix="installed-libcamera-", dir=args.build) as folder:
        prefix = Path(folder).resolve()
        subprocess.run([args.cmake, "--install", str(args.build), "--config", args.config,
                        "--prefix", str(prefix)], check=True, env=environment,
                       stdout=subprocess.DEVNULL)
        assert (prefix / "vendor/libcamera/lib/libcamera.so").is_file()
        # Check the actual loaded image, so a compatible system copy cannot hide a missing RPATH.
        maps_check = (
            "from pathlib import Path; import sys; "
            "expected=str(Path(sys.argv[1])/'vendor/libcamera/lib/libcamera.so'); "
            "assert expected in Path('/proc/self/maps').read_text(), expected"
        )
        if args.python:
            python_environment = dict(environment, PYTHONPATH=str(prefix / "lang/python"))
            subprocess.run([args.python, "-c",
                            "import wse; assert wse.runtime_info()['components']['tmr']; " + maps_check,
                            str(prefix)], check=True, env=python_environment, cwd=prefix)
            print("installed Python import and bundled libcamera: PASS", flush=True)
        if args.node:
            code = (
                "const fs=require('node:fs'); const path=require('node:path'); "
                "require(path.join(process.argv[1],'lang/js')); "
                "const expected=path.join(process.argv[1],'vendor/libcamera/lib/libcamera.so'); "
                "if(!fs.readFileSync('/proc/self/maps','utf8').includes(expected)) throw Error(expected);"
            )
            subprocess.run([args.node, "-e", code, str(prefix)], check=True,
                           env=environment, cwd=prefix)
            print("installed Node import and bundled libcamera: PASS", flush=True)
        for library in args.library:
            assert Path(library).name == library
            subprocess.run([sys.executable, "-c",
                            "import ctypes, sys; ctypes.CDLL(sys.argv[2]); " + maps_check,
                            str(prefix), str(prefix / "bin" / library)], check=True,
                           env=environment, cwd=prefix)
            # JNI and C ABI load checks exercise ELF dependencies, not their managed API behavior.
            print(f"installed {library} load and bundled libcamera: PASS", flush=True)


if __name__ == "__main__":
    main()
