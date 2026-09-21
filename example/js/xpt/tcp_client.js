'use strict';

// Portable XPT TCP client: connect, report both endpoints, send, receive, peer check, disconnect.
//
// XPT provides no listener API, so the peer comes from the command line; point it at any TCP echo
// service. Without a reachable server the sample reports the structured connection error and
// returns without failing. Build with WSE_BUILD_XPT=ON and WSE_BUILD_NODE_BINDING=ON.
//
// XPT deliberately has no default timeout: every operation takes an explicit context so a caller
// can never wait forever by accident. These calls block the calling thread until they finish or
// the deadline elapses, so keep the deadline short on the main thread.
//
// Synchronous means synchronous: each call returns a value, not a promise, and nothing else on
// the loop runs while it is in flight. A context may also carry a CancellationSource, but on the
// main thread there is nobody left to call cancel() during the call, so cancellation is only
// useful when the operation runs in a Worker.
//
// The first argument is either the installed `lang/js` package directory or a built `wse.node`
// addon. Given the addon the sample still loads the package, so what is exercised is the public
// JavaScript surface an application uses rather than the raw native entry points.
//
//   node example/js/xpt/tcp_client.js <installed lang/js directory or wse.node> <host> <port>
//
// Argument order: [2] package or addon, [3] host, [4] TCP port.

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

if ( wse.TcpClient === undefined ) {
  throw new Error('This WSE build does not include the XPT component.');
}

// The ceiling for one receive, not a length to wait for. A stream has no message boundaries, so a
// larger ceiling only means more bytes may arrive at once; whatever is left stays buffered for the
// next receive. Framing a message is the caller's job, not the transport's.
const RECEIVE_LIMIT_BYTES = 256;

async function main() {
  const host = process.argv[3];
  const port = Number(process.argv[4]);
  // The port is range-checked here rather than left to the binding, because a caller who typed a
  // wrong number deserves this message and not an InvalidArgument out of the transport.
  if ( !host || !Number.isInteger(port) || port <= 0 || port > 65535 ) {
    // TCP port 7 is the conventional echo service port.
    // Deliberate: with no peer named there is nothing to connect to, and the sample exits with
    // success so it runs on a machine with no service listening.
    console.log('Pass a host and a TCP port to connect to, for example 127.0.0.1 7.');
    return;
  }

  // One explicit deadline is reused for every operation in this sample.
  // Reused, not shared: each call gets a fresh 1000 ms, and the object carries no state between
  // them. A second is ample on a loopback or LAN service and short enough that an unreachable
  // host does not leave the sample sitting on the main thread; a distant host may need more.
  const context = { timeoutMs: 1000 };

  // The object owns the socket from connect() until disconnect(). JavaScript has no deterministic
  // destructor, so nothing closes that socket on its own: skip disconnect() and it stays open
  // until the garbage collector finalizes this object.
  const client = new wse.TcpClient();
  try {
    client.connect(host, port, context);
  } catch (error) {
    // Returning here rather than entering the try/finally below is safe precisely because a
    // failed connect leaves no socket to disconnect.
    // No server at that address is a reportable state, not a sample defect.
    console.log(`connect ${host}:${port} failed:`,
      error.category, error.code, error.nativeCode, error.message);
    console.log('Point the sample at a reachable TCP service.');
    return;
  }

  try {
    // The local endpoint is only known after a successful connect: the port in it is the ephemeral
    // one the operating system assigned. Both read from the live socket, so both are meaningless
    // once disconnect() has run.
    const local = client.localEndpoint();
    const remote = client.remoteEndpoint();
    console.log(`connected: ${local.host}:${local.port} -> ${remote.host}:${remote.port}`);

    // send() loops until the complete buffer is written or the deadline expires.
    // The returned count is therefore the whole payload on success; a short write is reported as
    // a failure rather than as a smaller number.
    const payload = Buffer.from('wse-xpt-tcp', 'utf8');
    console.log('sent bytes:', client.send(payload, context));

    // One receive returns a single chunk; an orderly peer close is RemoteClosed.
    // Against a peer that is not an echo service, TimedOut is the expected structured result
    // rather than a failure, so it is checked by code and the sample carries on to disconnect
    // cleanly and exit with success.
    try {
      const reply = client.receive(RECEIVE_LIMIT_BYTES, context);
      console.log(`received ${reply.length} bytes:`, reply.toString('utf8'));
    } catch (error) {
      if ( error.code === wse.TransportErrorCode.TimedOut ) {
        console.log('no reply within the deadline (the peer is not an echo service)');
      } else {
        console.log('receive reported:', error.code, error.message);
      }
    }

    // A non-destructive peek for an OS-observable peer close or reset.
    // It consumes nothing and answers immediately, so it takes no context of its own. It reports
    // only what the operating system already knows; a peer that has gone away silently still
    // looks connected until a send or receive proves otherwise.
    try {
      client.checkPeerConnection();
      console.log('peer check: no close or reset observed');
    } catch (error) {
      console.log('peer check: peer reported code', error.code);
    }
  } finally {
    // disconnect() is idempotent.
    // It is also not terminal: the same object may connect() again afterwards. Running it in a
    // finally is what closes the socket at a known moment even when a call above threw.
    client.disconnect();
  }
  console.log('disconnected');
}

main().catch((error) => {
  console.error(error.category, error.code, error.nativeCode, error.message);
  process.exitCode = 1;
});
