'use strict';

// Portable XPT UDP send and receive over the loopback interface with explicit deadlines.
//
// Hardware-free by design: both endpoints are UDP sockets bound to 127.0.0.1 in this process, so
// the sample runs anywhere XPT builds. Build with WSE_BUILD_XPT=ON and WSE_BUILD_NODE_BINDING=ON.
//
// XPT deliberately has no default timeout: every operation takes an explicit context so a caller
// can never wait forever by accident. These calls block the calling thread until they finish or
// the deadline elapses, so keep the deadline short on the main thread.
//
// Synchronous means synchronous: each call returns a value, not a promise, and nothing else on
// the loop runs while it is in flight. That is why main() below needs no async at all, unlike the
// camera samples. A context may also carry a CancellationSource, but on the main thread there is
// nobody left to call cancel() during the call, so cancellation is only useful in a Worker.
//
// The argument is either the installed `lang/js` package directory or a built `wse.node` addon.
// Given the addon the sample still loads the package, so what is exercised is the public
// JavaScript surface an application uses rather than the raw native entry points.
//
//   node example/js/xpt/udp_loopback.js <installed lang/js directory or wse.node>
//
// This sample takes no arguments of its own.

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

if ( wse.UdpClient === undefined ) {
  throw new Error('This WSE build does not include the XPT component.');
}

function main() {
  // One explicit deadline is reused for every operation in this sample.
  // Reused, not shared: each call gets a fresh 1000 ms, and the object carries no state between
  // them. A second is far more than a loopback datagram needs and short enough that a mistake
  // does not leave the sample sitting on the main thread.
  const context = { timeoutMs: 1000 };

  // Each object owns a socket from bind() until close(). JavaScript has no deterministic
  // destructor, so nothing releases those sockets on their own: skip close() and they stay bound,
  // holding their ports, until the garbage collector finalizes the objects.
  const receiver = new wse.UdpClient();
  const sender = new wse.UdpClient();
  try {
    // Port zero asks the operating system to assign a free port to each socket.
    // Naming a fixed port instead would make the sample fail wherever that port is already taken,
    // and the assigned port is readable afterwards through localEndpoint().
    // The sender is bound as well, so its datagrams carry a real source the receiver can report.
    receiver.bind('127.0.0.1', 0, context);
    sender.bind('127.0.0.1', 0, context);

    // Only meaningful after the bind: this is where the assigned port becomes known, and it is
    // what the send below aims at.
    const local = receiver.localEndpoint();
    console.log(`receiver endpoint: ${local.host}:${local.port}`);
    // The portable payload ceiling: a datagram larger than this is refused with MessageTooLarge
    // rather than fragmented, because what survives a real network is not what one host allows.
    console.log('maximum datagram size:', wse.UdpClient.maximumDatagramSize());

    const message = Buffer.from('wse-xpt-udp', 'utf8');
    console.log('sent bytes:', sender.sendTo(local.host, local.port, message, context));

    // The ceiling is the maximum rather than the eleven bytes actually expected, because a
    // datagram is delivered whole or not at all: a ceiling below the datagram's size yields
    // DatagramTruncated with a partial payload instead of a smaller successful read. Sizing the
    // buffer by the protocol maximum is the safe default when the sender is not known.
    const datagram = receiver.receiveFrom(wse.UdpClient.maximumDatagramSize(), context);
    // UDP is connectionless, so the source travels with the payload rather than with the socket.
    console.log(`received from: ${datagram.source.host}:${datagram.source.port}`);
    console.log('received payload:', datagram.payload.toString('utf8'));

    sender.sendTo(local.host, local.port, Buffer.from('prefix-and-suffix'), context);
    try {
      receiver.receiveFrom(3, context);
      throw new Error('Expected a truncated datagram');
    } catch (error) {
      if (error.code !== wse.TransportErrorCode.DatagramTruncated) throw error;
      if (error.bytesTransferred !== 3 || error.receivedData.toString() !== 'pre') throw error;
      console.log('truncated prefix:', error.receivedData.toString());
      // sourceEndpoint identifies the sender; the discarded suffix cannot be read again.
    }

    // A short deadline with no traffic must report TimedOut rather than blocking.
    try {
      receiver.receiveFrom(wse.UdpClient.maximumDatagramSize(), { timeoutMs: 50 });
      console.log('idle receive timed out: false');
    } catch (error) {
      console.log('idle receive timed out:', error.code === wse.TransportErrorCode.TimedOut);
    }
  } finally {
    // close() ends the session and is idempotent; the object stays usable for a later open().
    receiver.close();
    sender.close();
  }
}

main();
