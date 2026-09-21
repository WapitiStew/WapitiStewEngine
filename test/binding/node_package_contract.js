'use strict';

// Public-surface contract for the installed JavaScript package.
//
// The addon defines its entry points with `napi_default`, which makes them non-enumerable. A
// package entry point that re-exported them with an object spread would silently drop every native
// function while still loading, so this contract asserts the documented surface is actually
// reachable through `require('<sdk>/lang/js')`.

const assert = require('node:assert/strict');
const path = require('node:path');

process.env.WSE_NODE_ADDON = path.resolve(process.argv[2]);
const wse = require(path.resolve(process.argv[3]));

async function main() {
  // The functions the how-to and build guide tell callers to use.
  for (const name of ['runtimeInfo', 'copyFrame', 'wait', 'runAfter']) {
    assert.equal(typeof wse[name], 'function', `${name} must be reachable from the package`);
  }

  const info = wse.runtimeInfo();
  assert.match(info.version, /^\d+\.\d+\.\d+$/);
  assert.equal(info.bindingAbiVersion, 1);
  assert.deepEqual(
    Object.keys(info.components).sort(),
    ['gef', 'iui', 'oui', 'tmr', 'vpj', 'xpt'],
  );

  const copied = await wse.copyFrame(Buffer.from([1, 2, 3]));
  assert.deepEqual([...copied], [1, 2, 3]);

  // Every component owner the package advertises is a constructible class, and no internal
  // underscore-prefixed helper leaks into the public surface.
  const owners = ['WebCamera'];
  if (info.components.iui) owners.push('Keyboard');
  if (info.components.xpt) owners.push('TcpClient', 'UdpClient', 'SerialPort', 'CancellationSource');
  if (info.components.vpj) owners.push('ModelProfile', 'ModelProfileRegistry', 'ProfileControlSession');
  for (const name of owners) {
    assert.equal(typeof wse[name], 'function', `${name} must be exported`);
  }
  assert.ok(
    Object.keys(wse).every((name) => !name.startsWith('_')),
    'no internal helper may be exported',
  );

  // The image operations are free functions on the package, so a caller reaching them through
  // the package entry point is what this asserts; the addon names them with a leading underscore.
  for (const name of ["isFrameOperationSupported", "isFrameAveragingSupported", "isBayerFormat",
                      "bayerPatternOf", "applyOrientation", "demosaicFrame", "averageFrames"]) {
    assert.strictEqual(typeof wse[name], "function", name);
  }
  for (const name of ["ImageOrientation", "BayerPattern", "DemosaicMethod",
                      "CameraPixelFormat", "CameraControl"]) {
    assert.strictEqual(typeof wse[name], info.components.tmr ? "object" : "undefined", name);
  }
  if (info.components.tmr) {
    assert.strictEqual(wse.CameraPixelFormat.Bayer16Gbrg, 14);
    assert.strictEqual(wse.CameraPixelFormat.Uyvy422, 15);
    assert.strictEqual(wse.CameraControl.FrameRate, 18);
  }

  // The removed camera binding must stay absent.
  assert.equal(wse.CameraSession, undefined);
  assert.equal(wse.createCameraSession, undefined);

  console.log('node package contract passed');
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
