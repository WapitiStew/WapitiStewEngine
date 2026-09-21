"""Portable IUI keyboard readiness check and one consistent key snapshot.

Hardware-free by design: a machine without a readable keyboard reports that through the
readiness state instead of failing. Build with WSE_BUILD_IUI=ON and WSE_BUILD_PYTHON_BINDING=ON.

Constructing a Keyboard starts a monitoring thread, so readiness is not immediate: a caller must
poll `is_available` or `access_state` before its first read rather than assume the first snapshot
will work. `close()`, which `__exit__` calls, drops the native keyboard and ends that thread at a
point the caller chose; without it the thread would live until the interpreter collected the
object. There is no reopen: a closed Keyboard raises on every later call, so a second reading
needs a second object.

The C++ counterpart reports through the WSE log facility. The binding exposes no logging, so this
sample prints; it also has no way to turn a readiness state into a structured error on its own, so
it lets `snapshot()` raise and reads the reason off the exception.

With no argument the sample imports the installed `wse` package; with an argument it loads the
built extension module from that path instead.

    python example/python/iui/keyboard.py [path to the built _wse module]
"""

import importlib.util
import sys
import time
from pathlib import Path


def load_wse():
    if len(sys.argv) == 1:
        import wse

        return wse
    module_path = Path(sys.argv[1]).resolve()
    spec = importlib.util.spec_from_file_location("_wse", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load WSE Python module: {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    wse = load_wse()
    # A build without WSE_BUILD_IUI registers no Keyboard at all, so the name has to be looked for
    # rather than caught; there is no stub that raises a friendlier error later.
    if not hasattr(wse, "Keyboard"):
        raise SystemExit("This WSE build does not include the IUI component.")

    # The context manager is the single owner; leaving it stops the monitoring thread.
    with wse.Keyboard() as keyboard:
        # The backend needs a moment before it reports its first reading. Twenty tries at 25 ms is
        # a half-second ceiling: long enough that a working keyboard is never reported as STARTING,
        # short enough that a machine which will never have one is not made to wait. The loop exits
        # on the first success, so the ceiling only costs time when there is nothing to read.
        for _ in range(20):
            if keyboard.is_available:
                break
            time.sleep(0.025)

        print("keyboard state:", keyboard.access_state)

        if not keyboard.is_available:
            # Not a failure of this sample: snapshot() explains why the keyboard is unreadable.
            # UNAVAILABLE, PERMISSION_DENIED, and DISCONNECTED all land here, and each is a real
            # state of the machine rather than a defect. Returning 0 keeps a headless or sandboxed
            # runner green; branch on the error's category and code if a caller needs to tell them
            # apart, never on the message text.
            try:
                keyboard.snapshot()
            except wse.WseError as failure:
                print("keyboard is not readable:", failure)
            return 0

        # One snapshot is one consistent point in time; do not combine several separate reads.
        # It is a plain dictionary of bool lists owned by the caller, with the fixed lengths
        # KEYBOARD_ASCII_COUNT, KEYBOARD_FUNCTION_COUNT and friends, so it stays readable after the
        # keyboard is closed. pressed_ascii() below is a separate read and can disagree with it.
        snapshot = keyboard.snapshot()
        for group in ("ascii", "function", "arrow", "lock", "command"):
            print(f"pressed {group} keys:", sum(snapshot[group]))
        print("pressed ascii code:", keyboard.pressed_ascii())

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
