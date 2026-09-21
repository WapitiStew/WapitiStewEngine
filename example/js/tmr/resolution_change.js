'use strict';

// TMR resolution change: the frame size is fixed at open(), so this sample captures one frame at
// the first stream profile, closes the session, reopens it at a profile with a different frame
// size, and captures again.
//
// Hardware-aware by design: a machine with no camera, or a camera that advertises a single
// resolution, says so in one sentence and returns without failing. Build with WSE_BUILD_TMR=ON
// and WSE_BUILD_NODE_BINDING=ON.
//
// The contract worth taking away: there is no setResolution(). A stream configuration belongs to
// a session, so changing the frame size means ending the session and opening a new one. That is
// what makes close() non-terminal here - the same object carries both sessions.
//
// The argument is either the installed `lang/js` package directory or a built `wse.node` addon.
// Given the addon the sample still loads the package, so what is exercised is the public
// JavaScript surface an application uses rather than the raw native entry points.
//
//   node example/js/tmr/resolution_change.js <installed lang/js directory or wse.node>
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

if ( wse.CameraPixelFormat === undefined ) {
  throw new Error('This WSE build does not include the TMR component.');
}

// Each read gets this deadline of its own. Two seconds covers the slow first frame of a freshly
// opened session, which is exactly what both captures below ask for; a small value would time out
// while the device is still starting its stream rather than reporting a real stall.
const READ_TIMEOUT_MS = 2000;

// A WSE enumeration reaches JavaScript as a plain object of number-valued properties, so a
// readable report has to search it for the name behind a value.
function enumName(enumeration, value) {
  const entry = Object.entries(enumeration).find(([, number]) => number === value);
  return entry ? entry[0] : value;
}

function describeFormat(format) {
  return `${format.width}x${format.height}`;
}

// The C++ sample saves each capture as a BMP named after its resolution. The Node binding exposes
// no image writer, so each capture is reported on the console instead.
//
// close() is a session close, not a farewell: the same WebCamera object is reopened at the second
// profile, exactly as the C++ sample reopens one camera.
//
// open() sits outside the try on purpose: a failed open leaves no session to close, and the
// try/finally exists to guarantee that a session which did start is ended even when the read
// throws. The returned frame survives that close, because its `data` is a Buffer owned by
// JavaScript rather than a view onto the device's memory.
async function captureOnce(camera, device, configuration) {
  camera.open(device, configuration);
  try {
    camera.start();
    const frame = await camera.readFrame(READ_TIMEOUT_MS);
    // rowStride is reported alongside the size because it is not always width times the pixel
    // size: a backend may pad each row, and walking the buffer by width would shear the image.
    console.log(`${describeFormat(configuration.nativeFormat)}:`
      + ` frame=${frame.width}x${frame.height}`
      + ` format=${enumName(wse.CameraPixelFormat, frame.pixelFormat)}`
      + ` stride=${frame.rowStride} bytes=${frame.data.length}`);
    return frame;
  } finally {
    // close() here is what makes the next open() legal; it is safe to call twice and does not
    // retire the object. Skip it and the device would stay held until the garbage collector
    // finalizes the camera, so the second open() would be competing with the first session.
    if ( camera.isStreaming() ) camera.stop();
    camera.close();
  }
}

async function main() {
  const devices = wse.WebCamera.enumerate();
  if ( devices.length === 0 ) {
    console.log('No USB/UVC web camera is attached.');
    return;
  }

  // One device is enough for this sample: the point is two profiles on one camera, so the first
  // enumerated device is taken and its advertised profiles are searched.
  const device = devices[0];
  const capability = wse.WebCamera.capabilities(device);

  // Two profiles with different frame sizes; both must offer a usable output format.
  const first = capability.streamProfiles.find(
    (profile) => profile.outputFormats.length !== 0);
  if ( !first ) {
    console.log('The camera has no usable stream profile.');
    return;
  }
  // allowConversion lets TMR bridge the device's native format to the requested output, for
  // instance decoding MJPEG into BGR8. False would demand that the device deliver the output
  // format itself, which most profiles cannot, and open() would fail instead of converting.
  const firstConfiguration = {
    nativeFormat: first.nativeFormat,
    outputFormat: first.outputFormats[0],
    allowConversion: true,
  };

  // A different frame size is the whole point, so a profile that only differs in frame rate or
  // pixel format does not qualify. There may be none, which the sample reports below.

  const second = capability.streamProfiles.find((profile) => profile.outputFormats.length !== 0
    && (profile.nativeFormat.width !== first.nativeFormat.width
      || profile.nativeFormat.height !== first.nativeFormat.height));

  const camera = new wse.WebCamera();
  console.log('first resolution:', describeFormat(firstConfiguration.nativeFormat));
  const firstFrame = await captureOnce(camera, device, firstConfiguration);

  if ( !second ) {
    console.log('The camera advertises only one resolution; nothing to change.');
    return;
  }
  // Keep the first output format when the second profile also offers it.
  // Holding the pixel format steady leaves the frame size as the only difference between the two
  // captures, which is what the sample is demonstrating.
  const secondConfiguration = {
    nativeFormat: second.nativeFormat,
    outputFormat: second.outputFormats.includes(firstConfiguration.outputFormat)
      ? firstConfiguration.outputFormat
      : second.outputFormats[0],
    allowConversion: true,
  };
  console.log('second resolution:', describeFormat(secondConfiguration.nativeFormat));
  // The first capture closed the session; the same object opens again at the second profile.
  const secondFrame = await captureOnce(camera, device, secondConfiguration);

  console.log(`captured both resolutions: ${firstFrame.width}x${firstFrame.height}`
    + ` and ${secondFrame.width}x${secondFrame.height}`);
}

main().catch((error) => {
  console.error(error.category, error.code, error.nativeCode, error.message);
  process.exitCode = 1;
});
