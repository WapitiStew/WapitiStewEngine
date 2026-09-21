'use strict';

// Portable XPT serial port: open with fixed 8N1 framing, send, receive with a deadline, close.
//
// The device name comes from the command line; without that device the sample reports the
// structured OpenFailed error and returns without failing, which is itself the documented
// no-device behavior. Build with WSE_BUILD_XPT=ON and WSE_BUILD_NODE_BINDING=ON.
//
// XPT deliberately has no default timeout: every operation takes an explicit context so a caller
// can never wait forever by accident. These calls block the calling thread until they finish or
// the deadline elapses, so keep the deadline short on the main thread.
//
//   node example/js/xpt/serial_send_receive.js <installed lang/js directory or wse.node> \
//     <device name> [baud rate]
//
// Argument order: [2] package or addon, [3] device name, [4] optional baud rate.
// Windows uses "COM3"-style names; Linux uses "/dev/ttyUSB0"-style paths.
//
// 8N1 is not a choice this sample makes: eight data bits, no parity, one stop bit is the only
// framing the portable API offers, so the device name and the baud rate are the whole
// configuration. The legacy connector, which does expose framing, is not bound at all.
//
// The first argument is either the installed `lang/js` package directory or a built `wse.node`
// addon. Given the addon the sample still loads the package, so what is exercised is the public
// JavaScript surface an application uses rather than the raw native entry points.

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

if ( wse.SerialPort === undefined ) {
  throw new Error('This WSE build does not include the XPT component.');
}

// One of the supported rates: 1200/2400/4800/9600/19200/38400/57600/115200.
// The fastest of them is the default because it is the common one for modern USB adapters. A rate
// outside that set is refused at open(); a rate the peer does not share opens successfully and
// then exchanges nothing but noise, which no error can report.
const DEFAULT_BAUD_RATE = 115200;
// The ceiling for one receive, not a length to wait for. Whatever arrived within the deadline is
// returned, up to this many bytes, and anything beyond stays buffered for the next receive.
const RECEIVE_LIMIT_BYTES = 256;

async function main() {
  const deviceName = process.argv[3];
  const baudRate = process.argv[4] === undefined ? DEFAULT_BAUD_RATE : Number(process.argv[4]);
  if ( !deviceName || !Number.isInteger(baudRate) || baudRate <= 0 ) {
    // Deliberate: a device name cannot be guessed and differs per platform, so the sample says
    // what it needs and exits with success rather than failing on a machine with no serial port.
    console.log('Pass a serial device name and an optional baud rate, for example COM3 115200.');
    return;
  }

  // One explicit deadline is reused for every operation in this sample.
  // Reused, not shared: each call gets a fresh 1000 ms, and the object carries no state between
  // them. A second is long for a byte at 115200 baud and short enough that a wrong device name
  // does not leave the sample sitting on the main thread.
  const context = { timeoutMs: 1000 };

  // The object owns the OS handle from open() until close(). JavaScript has no deterministic
  // destructor, so nothing releases that handle on its own: skip close() and the port stays
  // locked against other processes until the garbage collector finalizes this object.
  const port = new wse.SerialPort();
  try {
    port.open(deviceName, baudRate, context);
  } catch (error) {
    // Returning here rather than entering the try/finally below is safe precisely because a
    // failed open leaves nothing open to close.
    // No such device on this machine is a reportable state, not a sample defect.
    console.log(`open ${deviceName} failed:`,
      error.category, error.code, error.nativeCode, error.message);
    console.log('Pass a port that exists on this machine.');
    return;
  }

  try {
    console.log(`opened ${port.deviceName()} at ${port.baudRate()} baud (8N1)`);

    // A serial line carries bytes, not text, so the payload is encoded explicitly. The trailing
    // carriage return is what most devices treat as end of command. send() returns the number of
    // bytes written and keeps writing until the buffer is gone or the deadline expires.
    const payload = Buffer.from('wse-xpt-serial\r', 'utf8');
    console.log('sent bytes:', port.send(payload, context));

    // One receive returns whatever the peer produced within the deadline. Without a loopback plug
    // or an answering device, TimedOut is the expected structured result.
    // That is a result, not a failure: it is checked by code rather than by message text, and the
    // sample carries on to close cleanly and exit with success.
    try {
      const reply = port.receive(RECEIVE_LIMIT_BYTES, context);
      console.log(`received ${reply.length} bytes:`, reply.toString('utf8'));
    } catch (error) {
      if ( error.code === wse.TransportErrorCode.TimedOut ) {
        console.log('no reply within the deadline (expected without an answering device)');
      } else {
        console.log('receive reported:', error.code, error.message);
      }
    }
  } finally {
    // close() ends the session and is idempotent; the object stays usable for a later open().
    // This is the only thing that releases the OS handle at a known moment, which is why it runs
    // in a finally rather than after the last console.log.
    port.close();
  }
  console.log('closed');
}

main().catch((error) => {
  console.error(error.category, error.code, error.nativeCode, error.message);
  process.exitCode = 1;
});
