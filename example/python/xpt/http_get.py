"""Portable XPT HTTP client: one GET request with the binding's default execution options.

The URL comes from the command line. Without it, or without network access, the sample reports that
in one sentence and exits cleanly. Build with WSE_BUILD_XPT=ON and WSE_BUILD_PYTHON_BINDING=ON.

The C++ counterpart still reads result.value() after a failure, so a 4xx/5xx answer keeps its
status code. The binding raises instead, but the answer is not thrown away: a 4xx/5xx raises
WseHttpStatusError, a WseError subclass whose `response` attribute is the same HttpResponse a
success would have returned. Every other failure raises a plain WseError with no response. Branch
on category and code, not on the message text.

    python example/python/xpt/http_get.py <path to the built _wse module> <url>
"""

import importlib.util
import sys
from pathlib import Path

TIMEOUT_MS = 5000
# The same 8 MiB ceiling the C++ default execution options use.
MAXIMUM_BODY_BYTES = 8 * 1024 * 1024
BODY_PREVIEW_BYTES = 80


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


def report_transport_error(operation: str, failure) -> None:
    print(
        f"ERROR: {operation} failed. category={failure.category} code={failure.code}"
        f" native={failure.native_code} message={failure}"
    )


def main() -> int:
    if len(sys.argv) < 3:
        print(
            "No URL was given; pass the built _wse module path, then a URL:"
            " python example/python/xpt/http_get.py <module path> <url>"
        )
        return 0
    url = sys.argv[2]

    wse = load_wse()
    if not hasattr(wse, "http_execute"):
        raise SystemExit("This WSE build does not include the XPT component.")

    # XPT deliberately has no default timeout: every operation takes an explicit deadline.
    context = wse.OperationContext(TIMEOUT_MS)
    request = wse.HttpRequest(wse.HttpMethod.GET, url)

    try:
        response = wse.http_execute(request, MAXIMUM_BODY_BYTES, context)
    except wse.WseError as failure:
        # An unreachable host and a 4xx/5xx answer both arrive here; the category and code tell
        # them apart. No network on this machine is a reportable state, not a sample defect.
        report_transport_error(f"GET {url}", failure)
        # WseHttpStatusError carries the answer the server did send, so a failed status is still
        # readable. Any other failure has no response to show.
        answer = getattr(failure, "response", None)
        if answer is not None:
            print(
                f"the server answered anyway: status={answer.status_code}"
                f" headers={len(answer.headers())} body bytes={len(answer.body())}"
            )
        return 0

    body = response.body()
    print(f"GET {url}: status={response.status_code} attempts={response.attempt_count}")
    print("header count:", len(response.headers()))
    print("body bytes:", len(body))
    print("body preview:", body[:BODY_PREVIEW_BYTES].decode("utf-8", "replace"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
