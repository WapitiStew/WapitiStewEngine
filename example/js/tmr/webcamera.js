'use strict';

// TMR web camera: enumerate the attached cameras, pick the first one that advertises a usable
// stream profile, open a session on it, and capture a single frame.
//
// This is the shortest complete path through the camera API, and the order is not negotiable:
// enumerate() names devices, capabilities() describes one of them, open() fixes the stream format
// for the whole session, start() begins delivery, and only then does readFrame() answer.
//
// Hardware-aware by design: a machine with no camera says so in one sentence and returns without
// failing, so the sample runs anywhere. A camera that enumerates but advertises no usable profile
// is a different matter and is raised as an error, matching the C++ counterpart's exit code.
// Build with WSE_BUILD_TMR=ON and WSE_BUILD_NODE_BINDING=ON.
//
// The argument is either the installed `lang/js` package directory or a built `wse.node` addon.
// Given the addon the sample still loads the package, so what is exercised is the public
// JavaScript surface an application uses rather than the raw native entry points.
//
//   node example/js/tmr/webcamera.js <installed lang/js directory or wse.node>
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

async function main() {
  // enumerate() takes no argument here, which means CameraBackend.Automatic: every backend the
  // platform offers. Passing MediaFoundation, Video4Linux2, or Libcamera restricts the search to
  // that one. An empty list is a normal answer, not an error.
  const devices = wse.WebCamera.enumerate();
  if ( devices.length === 0 ) {
    console.log('No USB/UVC web camera is attached.');
    return;
  }

  // capabilities() is static and queries the device rather than a session, so the format can be
  // chosen before anything is opened. A device may enumerate and still advertise nothing usable,
  // so every candidate is tried instead of trusting the first.
  let device;
  let profile;
  for ( const candidate of devices ) {
    const capability = wse.WebCamera.capabilities(candidate);
    const candidateProfile = capability.streamProfiles[0];
    if ( candidateProfile && candidateProfile.outputFormats.length !== 0 ) {
      device = candidate;
      profile = candidateProfile;
      break;
    }
  }
  if ( !device ) {
    throw new Error('No enumerated camera has a usable stream profile.');
  }

  // The object owns the native device from open() until close(). JavaScript has no deterministic
  // destructor, so the try/finally is the whole release story: skip close() and the device stays
  // held, and unavailable to every other process, until the garbage collector finalizes this
  // object at some unpredictable later moment.
  const camera = new wse.WebCamera();
  try {
    // The configuration is fixed for the whole session. nativeFormat is what the device is asked
    // to produce, outputFormat what the caller wants back, and allowConversion lets TMR bridge
    // the two, for instance decoding MJPEG into BGR8. Set it false and a device that cannot
    // deliver outputFormat itself makes open() fail rather than paying for a conversion.
    camera.open(device, {
      nativeFormat: profile.nativeFormat,
      outputFormat: profile.outputFormats[0],
      allowConversion: true,
    });
    // start() with no argument is the pull form: frames are fetched by readFrame(). Passing a
    // function instead switches to the push form, where the session's worker thread delivers each
    // frame through the loop.
    camera.start();
    // readFrame() resolves a promise from a libuv worker thread, so the await yields rather than
    // blocking. The 2000 ms deadline is sized for a first frame, which is the slow one: a UVC
    // camera needs time to start its stream, and a much smaller value times out before the sensor
    // has delivered anything. The frame's `data` is a copy owned by JavaScript, so it stays valid
    // after stop() and close().
    const frame = await camera.readFrame(2000);
    console.log(`${frame.width}x${frame.height} bytes=${frame.data.length}`);
    camera.stop();
  } finally {
    // Reached on the ordinary path too, where streaming has already stopped; stop() is only
    // repeated when readFrame() threw. close() ends the session, is safe to call twice, and
    // leaves the object usable, so a later open() would start a new session on this same camera.
    if ( camera.isStreaming() ) camera.stop();
    camera.close();
  }
}

main().catch((error) => {
  console.error(error.category, error.code, error.nativeCode, error.message);
  process.exitCode = 1;
});
