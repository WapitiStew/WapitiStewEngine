'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const binding = require(process.argv[2]);

const fixture = Object.fromEntries(fs.readFileSync(process.argv[3], 'utf8')
  .split(/\r?\n/).filter((line) => line && !line.startsWith('#'))
  .map((line) => { const split = line.indexOf('='); return [line.slice(0, split), line.slice(split + 1)]; }));
const number = (name) => Number.parseInt(fixture[name], 10);

const info = binding.runtimeInfo();
assert.equal(info.components.tmr, true);
assert.equal(binding.CameraBackend.Automatic, 0);
assert.equal(binding.CameraPixelFormat.Bgra8, 4);
assert.equal(binding.CameraControl.Exposure, 0);
assert.equal(binding.CameraControlMode.Automatic, 1);
assert.equal(binding.CameraControlUnit.Microseconds, 1);
assert.equal(binding.CameraTransport.UsbUvc, 1);
assert.equal(binding.CameraControl.PowerLineFrequency, 17);

const fixtureDevice = {
  backend: number('device_backend'), id: fixture.device_id,
  displayName: fixture.device_display_name, transport: fixture.device_transport,
  transportType: number('device_transport_type'),
  usb: { vendorId: number('usb_vendor_id'), productId: number('usb_product_id'),
    serialNumber: fixture.usb_serial_number, uvcVersionBcd: number('usb_uvc_version_bcd') },
};
const fixtureProfile = {
  nativeFormat: { width: number('profile_width'), height: number('profile_height'),
    frameRateNumerator: number('profile_fps_numerator'),
    frameRateDenominator: number('profile_fps_denominator'),
    pixelFormat: number('profile_native_pixel_format') },
  outputFormats: [number('profile_output_pixel_format')],
};
const fixtureControl = { control: number('control'), minimum: number('control_minimum'),
  maximum: number('control_maximum'), step: number('control_step'),
  defaultValue: number('control_default'), unit: number('control_unit') };
assert.equal(fixtureDevice.transportType, binding.CameraTransport.UsbUvc);
assert.equal(fixtureProfile.nativeFormat.pixelFormat, binding.CameraPixelFormat.Yuyv422);
assert.equal(fixtureProfile.outputFormats[0], binding.CameraPixelFormat.Bgra8);
assert.equal(fixtureControl.control, binding.CameraControl.Exposure);

const devices = binding._webCameraDevices(binding.CameraBackend.Automatic);
assert.ok(Array.isArray(devices));

assert.throws(
  () => binding._webCameraCapabilities({
    backend: binding.CameraBackend.Automatic,
    id: '',
    displayName: '',
    transport: '',
  }),
  (error) => Number.isInteger(error.category) && error.category !== 0
    && Number.isInteger(error.code) && error.code !== 0,
);

const camera = binding._createWebCamera();
assert.equal(camera.isOpen(), false);
assert.equal(camera.isStreaming(), false);
assert.throws(() => camera.start(),
  (error) => error.category === number('closed_error_category')
    && error.code === number('not_open_error_code'));
let callbackCount = 0;
for (let attempt = 0; attempt < 2; ++attempt) {
  assert.throws(() => camera.start(() => { ++callbackCount; }),
    (error) => error.category === number('closed_error_category')
      && error.code === number('not_open_error_code'));
}
assert.equal(callbackCount, 0, 'a rejected start must never deliver a callback');
camera.close();
camera.close();

// -----------------------------------------------------------------------------------------------
// Frame operations
// -----------------------------------------------------------------------------------------------

// The formats added for a Bayer sensor reach JavaScript with the same names the C++ side uses.
for (const name of ['Gray16', 'Rgb16', 'Bgr16',
                    'Bayer16Rggb', 'Bayer16Bggr', 'Bayer16Grbg', 'Bayer16Gbrg', 'Uyvy422']) {
  assert.strictEqual(typeof binding.CameraPixelFormat[name], 'number', name);
}
assert.strictEqual(binding.CameraControl.FrameRate, 18);
assert.strictEqual(binding.ImageOrientation.Rotate90Cw, 1);
assert.strictEqual(binding.BayerPattern.Grbg, 2);
assert.strictEqual(binding.DemosaicMethod.Bilinear, 1);

// Orientation moves whole pixels; averaging combines a site with itself. A Bayer frame therefore
// refuses the first and accepts the second, and the two questions have two answers.
assert.ok(binding._isFrameOperationSupported(binding.CameraPixelFormat.Rgb8));
assert.ok(!binding._isFrameOperationSupported(binding.CameraPixelFormat.Bayer16Rggb));
assert.ok(binding._isFrameAveragingSupported(binding.CameraPixelFormat.Bayer16Rggb));
assert.ok(!binding._isFrameAveragingSupported(binding.CameraPixelFormat.Mjpeg));
assert.ok(binding._isBayerFormat(binding.CameraPixelFormat.Bayer16Bggr));
assert.ok(!binding._isBayerFormat(binding.CameraPixelFormat.Rgb8));
assert.strictEqual(binding._bayerPatternOf(binding.CameraPixelFormat.Bayer16Grbg), binding.BayerPattern.Grbg);
assert.throws(() => binding._bayerPatternOf(binding.CameraPixelFormat.Rgb8));

// 3x2 so that a quarter turn is visible in the extent.
const rgb = {
  width: 3,
  height: 2,
  pixelFormat: binding.CameraPixelFormat.Rgb8,
  data: Buffer.from(Array.from({ length: 18 }, (unused, index) => index + 1)),
};
const turned = binding._applyOrientation(rgb, binding.ImageOrientation.Rotate90Cw);
assert.strictEqual(turned.width, 2);
assert.strictEqual(turned.height, 3);
assert.strictEqual(turned.rowStride, 6);

const half = binding._applyOrientation(rgb, binding.ImageOrientation.Rotate180);
const back = binding._applyOrientation(half, binding.ImageOrientation.Rotate180);
assert.ok(back.data.equals(rgb.data));

// Averaging one frame with itself returns that frame.
const averaged = binding._averageFrames([rgb, rgb]);
assert.ok(averaged.data.equals(rgb.data));

// A Bayer frame converts to colour, and a quarter turn is refused until it has.
const bayer = {
  width: 4,
  height: 4,
  pixelFormat: binding.CameraPixelFormat.Bayer16Rggb,
  data: Buffer.alloc(4 * 4 * 2),
};
const colour = binding._demosaicFrame(bayer, binding.CameraPixelFormat.Rgb8);
assert.strictEqual(colour.pixelFormat, binding.CameraPixelFormat.Rgb8);
assert.strictEqual(colour.rowStride, 12);
const wide = binding._demosaicFrame(bayer, binding.CameraPixelFormat.Bgr16, binding.DemosaicMethod.Block2x2);
assert.strictEqual(wide.rowStride, 24);
assert.throws(() => binding._applyOrientation(bayer, binding.ImageOrientation.Rotate180));
assert.throws(() => binding._demosaicFrame(rgb, binding.CameraPixelFormat.Rgb8));

// Averaging a Bayer frame is allowed, which is what a sensor needs before the colour is raised.
assert.strictEqual(
  binding._averageFrames([bayer, bayer]).pixelFormat, binding.CameraPixelFormat.Bayer16Rggb);
