'use strict';

const assert = require('node:assert/strict');
const binding = require(process.argv[2]);

async function main() {
  const info = binding.runtimeInfo();
  assert.match(info.version, /^\d+\.\d+\.\d+$/);
  assert.equal(info.bindingAbiVersion, 1);
  assert.deepEqual(
    Object.keys(info.components).sort(),
    ['gef', 'iui', 'oui', 'tmr', 'vpj', 'xpt'],
  );
  for (const available of Object.values(info.components)) {
    assert.equal(typeof available, 'boolean');
  }

  const source = new Uint8Array([0, 1, 127, 255]);
  const pendingCopy = binding.copyFrame(source);
  source[1] = 99;
  const copied = await pendingCopy;
  assert.ok(Buffer.isBuffer(copied));
  assert.deepEqual([...copied], [0, 1, 127, 255]);

  const empty = await binding.copyFrame(Buffer.alloc(0));
  assert.ok(Buffer.isBuffer(empty));
  assert.equal(empty.length, 0);

  await binding.wait(1);
  const typed = new Uint16Array([0x1234, 0x5678]);
  const typedCopy = await binding.copyFrame(typed);
  assert.deepEqual(
    [...typedCopy],
    [...new Uint8Array(typed.buffer, typed.byteOffset, typed.byteLength)],
  );
  const dataViewBuffer = new ArrayBuffer(6);
  const dataView = new DataView(dataViewBuffer, 1, 4);
  dataView.setUint32(0, 0x12345678, true);
  const dataViewCopy = await binding.copyFrame(dataView);
  assert.deepEqual([...dataViewCopy], [0x78, 0x56, 0x34, 0x12]);
  assert.throws(() => binding.wait(-1), { name: 'RangeError' });

  await new Promise((resolve, reject) => {
    let timer;
    const timeout = setTimeout(() => reject(new Error('runAfter callback timed out')), 1000);
    timer = binding.runAfter(5, () => {
      clearTimeout(timeout);
      timer.close();
      resolve();
    });
  });

  let cancelledCallback = false;
  const cancelledTimer = binding.runAfter(50, () => {
    cancelledCallback = true;
  });
  cancelledTimer.cancel();
  cancelledTimer.close();
  await binding.wait(60);
  assert.equal(cancelledCallback, false);

  for (let index = 0; index < 50; ++index) {
    const frame = await binding.copyFrame(Buffer.from([index & 0xff]));
    assert.equal(frame[0], index & 0xff);
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
