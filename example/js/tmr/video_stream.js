'use strict';

// TMR video stream: reads ten consecutive frames from the first usable camera and reports each
// frame's sequence number and the time that passed since the previous frame.
//
// The C++ counterpart uses the push callback that runs on the session's worker thread. That form
// is bound too, as `start(callback)`, but a pull loop keeps the reporting ordered on the main
// thread and needs no completion signalling, so it is what this sample shows.
//
// The two forms are not equivalent under load. A pushed frame crosses to JavaScript through a
// threadsafe function with a four-deep queue, posted without blocking the camera's worker thread,
// so a loop that falls behind loses frames rather than stalling capture. A pull loop cannot lose
// a frame that way: each read waits for the next one, and a stream that stops delivering is
// reported as a timeout instead of as silence.
//
// The argument is either the installed `lang/js` package directory or a built `wse.node` addon.
// Given the addon the sample still loads the package, so what is exercised is the public
// JavaScript surface an application uses rather than the raw native entry points.
//
// Hardware-aware by design: a machine with no camera says so in one sentence and returns without
// failing. Build with WSE_BUILD_TMR=ON and WSE_BUILD_NODE_BINDING=ON.
//
//   node example/js/tmr/video_stream.js <installed lang/js directory or wse.node>
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

// Ten frames is enough to show a steady interval without holding the device for long: at a
// typical 30 fps this is a third of a second. Raising it lengthens the run proportionally and
// does not change what the sample demonstrates.
const TARGET_FRAMES = 10;
// Each read gets its own deadline, not a share of a total budget. Two seconds comfortably covers
// the slow first frame while a device starts its stream; a value below the frame interval would
// turn ordinary pacing into a timeout.
const READ_TIMEOUT_MS = 2000;
const NANOSECONDS_PER_MILLISECOND = 1000000;

function enumName(enumeration, value) {
  const entry = Object.entries(enumeration).find(([, number]) => number === value);
  return entry ? entry[0] : value;
}

async function main() {
  const devices = wse.WebCamera.enumerate();
  if ( devices.length === 0 ) {
    console.log('No USB/UVC web camera is attached.');
    return;
  }

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
    console.log('No enumerated camera has a usable stream profile.');
    return;
  }

  // The object owns the native device from open() until close(), and JavaScript has no
  // deterministic destructor, so the try/finally is the whole release story: skip close() and the
  // device stays held until the garbage collector finalizes this object.
  const camera = new wse.WebCamera();
  try {
    // The frame size and pixel format are fixed here for the whole session; allowConversion lets
    // TMR bridge the device's native format to the requested output, for instance MJPEG to BGR8.
    camera.open(device, {
      nativeFormat: profile.nativeFormat,
      outputFormat: profile.outputFormats[0],
      allowConversion: true,
    });
    // No callback, so this is the pull form; readFrame() below is the only way frames arrive.
    camera.start();

    // The loop reads strictly one frame at a time. Reads on one camera are serialized inside the
    // binding, so overlapping them would only queue them behind each other while making the
    // reported intervals meaningless.
    let previousTimestampNs = 0;
    let firstTimestampNs = 0;
    let received = 0;
    for ( let index = 0; index < TARGET_FRAMES; ++index ) {
      // Each read waits at most READ_TIMEOUT_MS; a stalled stream throws instead of hanging.
      // The wait happens on a libuv worker thread, so the await yields the loop rather than
      // blocking it, and `frame.data` is a copy owned by JavaScript that outlives this session.
      const frame = await camera.readFrame(READ_TIMEOUT_MS);
      // The timestamp is monotonic, so the difference between two frames is a real interval.
      // `sequence` comes from the device and is the thing to watch for dropped frames: a gap
      // there means the stream skipped, while a delta here only means this loop was late.
      const deltaMs = index === 0
        ? 0
        : (frame.monotonicTimestampNs - previousTimestampNs) / NANOSECONDS_PER_MILLISECOND;
      if ( index === 0 ) firstTimestampNs = frame.monotonicTimestampNs;
      previousTimestampNs = frame.monotonicTimestampNs;
      ++received;
      console.log(`frame ${received}/${TARGET_FRAMES}: sequence=${frame.sequence}`
        + ` timestamp_ns=${frame.monotonicTimestampNs} delta_ms=${deltaMs.toFixed(2)}`
        + ` ${frame.width}x${frame.height}`
        + ` format=${enumName(wse.CameraPixelFormat, frame.pixelFormat)}`
        + ` bytes=${frame.data.length}`);
    }

    // The mean is taken over the gaps between frames, one fewer than the frames themselves, so
    // the first frame's arrival does not count as an interval of its own.
    const elapsedMs = (previousTimestampNs - firstTimestampNs) / NANOSECONDS_PER_MILLISECOND;
    const intervals = received - 1;
    const meanMs = intervals > 0 ? elapsedMs / intervals : 0;
    console.log(`stream complete: ${received} frames over ${elapsedMs.toFixed(2)} ms,`
      + ` mean interval ${meanMs.toFixed(2)} ms`
      + ` (${meanMs > 0 ? (1000 / meanMs).toFixed(2) : '0.00'} fps)`);
  } finally {
    // Runs whether the loop finished or a read timed out. close() ends the session, is safe to
    // call twice, and leaves the object usable for a later open().
    if ( camera.isStreaming() ) camera.stop();
    camera.close();
  }
}

main().catch((error) => {
  console.error(error.category, error.code, error.nativeCode, error.message);
  process.exitCode = 1;
});
