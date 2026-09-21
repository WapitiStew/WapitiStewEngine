'use strict';

const assert = require('node:assert/strict');
const { Worker, isMainThread, parentPort, workerData } = require('node:worker_threads');

if (!isMainThread) {
  const binding = require(workerData.addonPath);
  assert.equal(binding.runtimeInfo().bindingAbiVersion, 1);
  binding.runAfter(10_000, () => {});
  binding.wait(10_000).catch(() => {});
  parentPort.postMessage('ready');
} else {
  async function runWorker(addonPath) {
    const worker = new Worker(__filename, { workerData: { addonPath } });
    await new Promise((resolve, reject) => {
      worker.once('message', resolve);
      worker.once('error', reject);
    });
    const started = Date.now();
    await worker.terminate();
    assert.ok(Date.now() - started < 1000, 'addon cleanup must cancel and join promptly');
  }

  async function main() {
    const addonPath = process.argv[2];
    for (let index = 0; index < 10; ++index) {
      await runWorker(addonPath);
    }
  }

  main().catch((error) => {
    console.error(error);
    process.exitCode = 1;
  });
}
