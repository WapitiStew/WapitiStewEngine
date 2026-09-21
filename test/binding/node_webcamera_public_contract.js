'use strict';

const assert = require('node:assert/strict');
const path = require('node:path');

process.env.WSE_NODE_ADDON = path.resolve(process.argv[2]);
const wse = require(path.resolve(process.argv[3]));

assert.equal(typeof wse.WebCamera, 'function');
assert.equal(wse.CameraSession, undefined);
assert.equal(wse.createCameraSession, undefined);
assert.equal(wse.cameraDevices, undefined);
assert.equal(wse._createWebCamera, undefined);

const devices = wse.WebCamera.enumerate();
assert.ok(Array.isArray(devices));

const camera = new wse.WebCamera();
assert.equal(camera.isOpen(), false);
assert.throws(() => camera.start(),
  (error) => Number.isInteger(error.category) && error.category !== 0);
camera.close();
camera.close();
assert.equal(camera.isOpen(), false);

// close() ends the session; it does not retire the object. A later open() must therefore be
// refused for the same reason a never-closed camera refuses it -- the device, not the state of
// the wrapper. Hardware-free: the device identity below cannot exist, so both attempts fail at
// the device, and the two failures must be identical.
const absentDevice = {
  backend: wse.CameraBackend.Automatic,
  id: 'closed-camera',
  displayName: 'Closed camera',
  transport: 'test',
  transportType: wse.CameraTransport.Unknown,
};

function openFailure(target) {
  try {
    target.open(absentDevice);
  } catch (error) {
    return error;
  }
  return null;
}

const fresh = new wse.WebCamera();
const freshFailure = openFailure(fresh);
const reopenFailure = openFailure(camera);
assert.ok(freshFailure !== null, 'an absent device must be refused');
assert.ok(reopenFailure !== null, 'an absent device must be refused after close()');
assert.notEqual(freshFailure.category, 0);
assert.equal(reopenFailure.category, freshFailure.category,
  'close() must not turn a later open() into a different, state-based refusal');
assert.equal(reopenFailure.code, freshFailure.code);
assert.equal(reopenFailure.message, freshFailure.message);
assert.doesNotMatch(reopenFailure.message, /closed|released/i,
  'a closed camera must not report itself as unusable');

// start() after close() must also behave like a fresh camera rather than a retired one, and a
// second close() stays safe.
assert.throws(() => camera.start(),
  (error) => Number.isInteger(error.category) && error.category !== 0);
camera.close();
fresh.close();
