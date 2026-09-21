"""Hardware-free WSE Python quick start.

It asks the binding what it is: version, binding ABI version, and one flag per component. Nothing
here opens a device or a socket, so it runs anywhere an extension module builds.

The `components` dictionary in the reply is the same information every other sample tests with
`hasattr()`. A build configured without a component never registers that component's names at all,
so a sample that needs one has to look before it calls.

The module arrives one of two ways. With no argument the sample imports the installed `wse`
package, which on Windows adds the sibling `bin` directory to the DLL search path before it pulls
`_wse` in. With an argument the sample loads the built extension module straight from that path,
which is what an uninstalled build tree needs; nothing prepares the search path in that case, so
the shared library the extension links against has to be findable on its own. Either object
answers `runtime_info()` the same way.

    python example/python/core/quickstart.py [path to the built _wse module]
"""

import importlib.util
import re
import sys
from pathlib import Path


def load_wse():
    # No argument selects the installed package; an argument selects the freshly built extension,
    # so a build tree can be exercised before anything is installed.
    if len(sys.argv) == 1:
        import wse

        return wse
    module_path = Path(sys.argv[1]).resolve()
    spec = importlib.util.spec_from_file_location("_wse", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load WSE Python module: {module_path}")
    module = importlib.util.module_from_spec(spec)
    # The importlib recipe: register the module under its own name before it executes, so anything
    # that later resolves `_wse` by name finds this instance instead of loading a second copy.
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    info = load_wse().runtime_info()
    # The version shape is part of the contract, so a build that cannot spell it is a failure
    # rather than something to print. Every other field is reported exactly as it arrives.
    if re.fullmatch(r"\d+\.\d+\.\d+", info["version"]) is None:
        raise RuntimeError("WSE returned an invalid semantic version")
    print(info)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
