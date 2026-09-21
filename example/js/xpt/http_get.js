'use strict';

// Portable XPT HTTP client: one GET request, reporting the status code, the attempt count, the
// header count, and the body size.
//
// The URL comes from the command line; without it, or without network access, the sample reports
// what it can and returns without failing. Build with WSE_BUILD_XPT=ON and
// WSE_BUILD_NODE_BINDING=ON.
//
// XPT deliberately has no default timeout: every operation takes an explicit context so a caller
// can never wait forever by accident. This call blocks the calling thread until it finishes or the
// deadline elapses, so keep the deadline short on the main thread.
//
// Synchronous means synchronous: httpExecute() returns a response, not a promise, and nothing
// else on the loop runs while it is in flight. A context may also carry a CancellationSource, but
// on the main thread there is nobody left to call cancel() during the call, so cancellation is
// only useful when the operation runs in a Worker.
//
// The first argument is either the installed `lang/js` package directory or a built `wse.node`
// addon. Given the addon the sample still loads the package, so what is exercised is the public
// JavaScript surface an application uses rather than the raw native entry points.
//
//   node example/js/xpt/http_get.js <installed lang/js directory or wse.node> <url>
//
// Argument order: [2] package or addon, [3] URL.

const path = require('node:path');

const target = process.argv[2] || process.env.WSE_JAVASCRIPT_PACKAGE;
if ( !target ) {
  throw new Error('Pass the installed lang/js directory, or the built wse.node addon.');
}
// A `.node` argument is the build-tree addon. Point the package loader at it and still require the
// package, so the sample always demonstrates the same public surface an application uses.
if ( target.endsWith('.node') ) process.env.WSE_NODE_ADDON = path.resolve(target);
const wse = require(target.endsWith('.node')
  ? path.resolve(__dirname, '..', '..', '..', 'lang', 'js')
  : path.resolve(target));

if ( wse.httpExecute === undefined ) {
  throw new Error('This WSE build does not include the XPT component.');
}

// The body ceiling is an argument rather than a default because the caller, not the server,
// decides how much memory a response may take. One MiB is ample for the pages a sample fetches;
// the C++ default is 8 MiB. A response larger than the ceiling fails the whole call with
// ResponseTooLarge rather than delivering a truncated body, so raise it for a real download.
const MAXIMUM_BODY_BYTES = 1024 * 1024;
// Console noise control only: the whole body is already in memory by this point.
const BODY_PREVIEW_BYTES = 80;

async function main() {
  const url = process.argv[3];
  if ( !url ) {
    // Deliberate: with no URL there is nothing to fetch, and a sample that cannot reach the
    // network should still run and exit with success rather than look like a failure.
    console.log('Pass a URL to fetch, for example http://example.com/.');
    return;
  }

  // One deadline covers the whole exchange - name lookup, connection, and transfer together - not
  // one stage of it. Five seconds is generous for a small page over a slow link; too small a
  // value turns an ordinary connection setup into TimedOut.
  const context = { timeoutMs: 5000 };
  // The binding never retries on the caller's behalf: a request is treated as non-idempotent and
  // transient-failure retries stay off, so attemptCount is expected to be 1.
  // `headers` and `body` are the other two fields a request may carry; a GET needs neither.
  const request = { method: wse.HttpMethod.Get, url };

  try {
    // A fourth argument, `{ username, secret }`, switches to the authenticated call and lets the
    // server negotiate the scheme. Without it the request is sent unauthenticated.
    const response = wse.httpExecute(request, MAXIMUM_BODY_BYTES, context);
    console.log(`GET ${url}: status=${response.statusCode}`
      + ` attempts=${response.attemptCount}`
      + ` headers=${response.headers.length}`
      + ` body_bytes=${response.body.length}`);
    const previewBytes = Math.min(response.body.length, BODY_PREVIEW_BYTES);
    console.log('body preview:', response.body.toString('utf8', 0, previewBytes));
  } catch (error) {
    // The binding raises a 4xx/5xx answer as an error rather than returning it, but the error
    // carries the received response on `error.response`, so the status code is still readable.
    // That property is set for HttpStatusError only. Branch on category and code, not on the
    // message text.
    //
    // Catching here rather than letting it reach the top-level handler is deliberate: an
    // unreachable host or a 404 is a result this sample is meant to show, not a defect, so it is
    // reported and the process still exits with success.
    console.log(`GET ${url} failed:`,
      error.category, error.code, error.nativeCode, error.message);
    console.log('http status error:', error.code === wse.TransportErrorCode.HttpStatusError);
    if ( error.response ) {
      console.log(`response on the error: status=${error.response.statusCode}`
        + ` attempts=${error.response.attemptCount}`
        + ` headers=${error.response.headers.length}`
        + ` body_bytes=${error.response.body.length}`);
    }
  }
}

main().catch((error) => {
  console.error(error.category, error.code, error.nativeCode, error.message);
  process.exitCode = 1;
});
