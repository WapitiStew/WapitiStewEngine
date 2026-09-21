import subprocess
import sys
from pathlib import Path


module_path = Path(sys.argv[1]).resolve()
program = r"""
import importlib.util
import sys
spec = importlib.util.spec_from_file_location('_wse', sys.argv[1])
if spec is None or spec.loader is None:
    raise RuntimeError('missing extension loader')
module = importlib.util.module_from_spec(spec)
sys.modules['_wse'] = module
spec.loader.exec_module(module)
for _ in range(50):
    with module.Runtime() as runtime:
        runtime.wait(0)
        assert bytes(runtime.copy_frame(b'x')) == b'x'
"""

for _ in range(10):
    completed = subprocess.run(
        [sys.executable, "-c", program, str(module_path)],
        check=False,
        capture_output=True,
        text=True,
        timeout=5,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"Python binding lifecycle subprocess failed: "
            f"{completed.stdout}{completed.stderr}"
        )
