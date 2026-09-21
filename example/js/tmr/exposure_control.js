'use strict';

// TMR exposure control: report the advertised exposure range, capture one frame, move the
// exposure to a different value inside that range, and capture a second frame so the change is
// visible.
//
// Hardware-aware by design: a machine with no camera, or a camera with no writable manual
// exposure, says so in one sentence and returns without failing. Build with WSE_BUILD_TMR=ON and
// WSE_BUILD_NODE_BINDING=ON.
//
// The contract worth taking away: never write a control value you did not derive from the
// capability. The advertised range, step, and access flags differ per device and per control, and
// a value outside them is refused rather than clamped. controlCapability() throws for a control
// the camera does not advertise at all, which is why the read below sits in its own try.
//
// The binding is narrower than the C++ API here. C++ offers readExposure()/writeExposure() and
// the capability's valueFromNormalized()/physicalFromValue() helpers; JavaScript has only the
// generic getControl()/setControl() pair and the raw capability numbers, so the arithmetic those
// helpers would do is written out below.
//
// The argument is either the installed `lang/js` package directory or a built `wse.node` addon.
// Given the addon the sample still loads the package, so what is exercised is the public
// JavaScript surface an application uses rather than the raw native entry points.
//
//   node example/js/tmr/exposure_control.js <installed lang/js directory or wse.node>
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

if ( wse.CameraControl === undefined ) {
  throw new Error('This WSE build does not include the TMR component.');
}

// Every read gets this deadline of its own, not a share of a total budget. Two seconds covers the
// slow first frame of a starting stream; a longer exposure than this would need a larger value,
// since a frame cannot arrive faster than its own exposure time.
const READ_TIMEOUT_MS = 2000;
// Frames discarded after the write so the sensor settles on the new exposure.
// A device applies a control change over the next frames rather than instantly, so reading too
// early compares the new setting against an image still exposed with the old one. Five is
// comfortable for the devices measured; fewer risks a misleading comparison, more only costs time.
const SETTLE_FRAMES = 5;

// A WSE enumeration reaches JavaScript as a plain object of number-valued properties, so a
// readable report has to search it for the name behind a value.
function enumName(enumeration, value) {
  const entry = Object.entries(enumeration).find(([, number]) => number === value);
  return entry ? entry[0] : value;
}

// The C++ sample saves a BMP before and after the change. The Node binding exposes no image
// writer, so this sample reports the mean byte of each frame instead: a cheap brightness proxy
// that works for any pixel format and makes the exposure change visible on the console.
//
// It is a proxy and not a measurement: the bytes are read raw, so a compressed or planar format
// gives a number that still moves with exposure but means nothing on its own.
function meanSample(frame) {
  if ( frame.data.length === 0 ) return 0;
  let total = 0;
  for ( let index = 0; index < frame.data.length; ++index ) {
    total += frame.data[index];
  }
  return total / frame.data.length;
}

// The C++ helpers valueFromNormalized() and physicalFromValue() are not bound, so the target is
// computed here from the advertised range and snapped onto the advertised step grid.
//
// The step guard matters: a device may advertise zero, and dividing by it would produce a value
// the device then refuses. Snapping also has to land inside [minimum, maximum], because rounding
// outward at either end would leave the range the capability promised.
function valueAtFraction(capability, fraction) {
  const step = capability.step > 0 ? capability.step : 1;
  const span = capability.maximum - capability.minimum;
  const raw = capability.minimum + (span * fraction);
  const aligned = capability.minimum + (Math.round((raw - capability.minimum) / step) * step);
  return Math.min(Math.max(aligned, capability.minimum), capability.maximum);
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
  // device stays held until the garbage collector finalizes this object. It also guarantees the
  // session is closed on every early return below, including the two that report an unsupported
  // control.
  const camera = new wse.WebCamera();
  try {
    camera.open(device, {
      nativeFormat: profile.nativeFormat,
      outputFormat: profile.outputFormats[0],
      allowConversion: true,
    });
    // Controls are read and written on an open session, and the stream has to be running for the
    // change to show up in a frame, so start() comes before any of the control work.
    camera.start();

    // The advertised capability carries the valid range, step, unit, and access.
    // A camera that does not implement this control makes the call throw rather than return a
    // blank capability, so the read is guarded and the sample reports the reason and stops.
    let exposure;
    try {
      exposure = camera.controlCapability(wse.CameraControl.Exposure);
    } catch (error) {
      console.log('This camera does not advertise an exposure control:', error.message);
      return;
    }
    console.log(`exposure capability: range=[${exposure.minimum},${exposure.maximum}]`
      + ` step=${exposure.step} unit=${enumName(wse.CameraControlUnit, exposure.unit)}`
      + ` manual=${exposure.supportsManual} writable=${exposure.writable}`);
    // A control can be advertised, readable, and still not writable, or writable only in
    // automatic mode. Both flags have to hold before a manual write is worth attempting; a
    // read-only exposure is a normal device, not a fault, so the sample stops with success.
    // `unit` and `physicalScale` say what the raw numbers mean physically, but no conversion
    // helper is bound, so the sample reports the unit and works in raw device values.
    if ( !exposure.writable || !exposure.supportsManual ) {
      console.log('This camera does not accept a manual exposure; nothing to change.');
      return;
    }

    // Read the current value first: it is both the baseline for the comparison and the setting
    // restored at the end, and a control value carries its mode alongside its number.
    const original = camera.getControl(wse.CameraControl.Exposure);
    const before = await camera.readFrame(READ_TIMEOUT_MS);
    const beforeMean = meanSample(before);
    console.log(`before: value=${original.value}`
      + ` mode=${enumName(wse.CameraControlMode, original.mode)}`
      + ` frame=${before.width}x${before.height} mean=${beforeMean.toFixed(2)}`);

    // Move to a clearly different position inside the advertised range.
    // A quarter and three quarters are far enough apart that the brightness visibly changes on
    // any device, and near enough the middle that neither lands on a range end where a device
    // might silently ignore the write. The fallback covers a camera already sitting at 0.25.
    let value = valueAtFraction(exposure, 0.25);
    if ( value === original.value ) {
      value = valueAtFraction(exposure, 0.75);
    }
    // Manual mode is part of the write, not a separate step: writing a value while the control is
    // in Automatic mode lets the device overwrite it again on the next frame.
    camera.setControl({
      control: wse.CameraControl.Exposure,
      mode: wse.CameraControlMode.Manual,
      value,
    });

    for ( let skipped = 0; skipped < SETTLE_FRAMES; ++skipped ) {
      await camera.readFrame(READ_TIMEOUT_MS);
    }

    // Report what the device accepted rather than what was asked for: a device may snap the value
    // onto its own grid, so the written number and the applied one are not always equal.
    const applied = camera.getControl(wse.CameraControl.Exposure);
    const after = await camera.readFrame(READ_TIMEOUT_MS);
    const afterMean = meanSample(after);
    console.log(`after: value=${applied.value}`
      + ` mode=${enumName(wse.CameraControlMode, applied.mode)}`
      + ` frame=${after.width}x${after.height} mean=${afterMean.toFixed(2)}`);
    console.log(`exposure ${original.value} -> ${applied.value},`
      + ` mean sample ${beforeMean.toFixed(2)} -> ${afterMean.toFixed(2)}`);

    // Restore the setting the device had before this sample ran.
    // A control write outlives the session and often the process: the device keeps it until
    // something else changes it. The value read at the start is passed back whole, mode included,
    // so a camera that was on automatic returns to automatic.
    camera.setControl(original);
    console.log('restored the original exposure');
  } finally {
    // Reached on every path, including the early returns above and a failed read. close() ends
    // the session, is safe to call twice, and leaves the object usable for a later open().
    if ( camera.isStreaming() ) camera.stop();
    camera.close();
  }
}

main().catch((error) => {
  console.error(error.category, error.code, error.nativeCode, error.message);
  process.exitCode = 1;
});
