"""IUI keyboard contract for the WSE Python binding.

Hardware-free: a machine without a readable keyboard must report that through the readiness
state and a structured error, never through a crash or a silently empty snapshot.
"""

import importlib.util
import sys
from pathlib import Path


def load_module(path: Path):
    spec = importlib.util.spec_from_file_location("_wse", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load WSE Python module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    wse = load_module(Path(sys.argv[1]).resolve())
    assert wse.runtime_info()["components"]["iui"] is True, "IUI must be reported as available"

    groups = {
        "ascii": wse.KEYBOARD_ASCII_COUNT,
        "function": wse.KEYBOARD_FUNCTION_COUNT,
        "arrow": wse.KEYBOARD_ARROW_COUNT,
        "lock": wse.KEYBOARD_LOCK_COUNT,
        "command": wse.KEYBOARD_COMMAND_COUNT,
    }
    assert groups["ascii"] == 128 and groups["function"] == 24
    assert groups["arrow"] == 4 and groups["lock"] == 3 and groups["command"] == 9

    with wse.Keyboard() as keyboard:
        state = keyboard.access_state
        assert isinstance(state, wse.KeyboardAccessState)
        assert isinstance(keyboard.is_available, bool)

        # Each observer is independent; startup/hotplug may happen between calls.
        categories = {0: 3, 2: 2, 3: 8, 4: 4}
        def verify_failure(failure):
            assert failure.code in categories, "Known failing keyboard state"
            assert failure.category == categories[failure.code]

        try:
            snapshot = keyboard.snapshot()
            assert set(snapshot) == set(groups)
            for name, count in groups.items():
                assert len(snapshot[name]) == count, name
                assert all(isinstance(value, bool) for value in snapshot[name]), name
        except wse.WseError as failure:
            verify_failure(failure)
        try:
            assert isinstance(keyboard.pressed_ascii(), int)
        except wse.WseError as failure:
            verify_failure(failure)

    keyboard = wse.Keyboard()
    keyboard.close()
    keyboard.close()
    assert keyboard.closed is True
    try:
        keyboard.snapshot()
    except wse.WseError as failure:
        assert failure.category == 3, "A closed keyboard must report an invalid state"
    else:
        raise AssertionError("A closed keyboard must be rejected")

    print("python IUI contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
