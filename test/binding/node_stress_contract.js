'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const binding = require(process.argv[2]);

const fixture = Object.fromEntries(fs.readFileSync(process.argv[3], 'utf8')
  .split(/\r?\n/)
  .filter((line) => line && !line.startsWith('#'))
  .map((line) => {
    const separator = line.indexOf('=');
    return [line.slice(0, separator), line.slice(separator + 1)];
  }));

function projectionRequest() {
  return {
    outputWidth: Number(fixture.output_width),
    outputHeight: Number(fixture.output_height),
    backend: process.platform === 'win32'
      ? binding.RendererBackend.Direct3D12
      : binding.RendererBackend.Vulkan12,
    useSoftwareAdapter: true,
    layers: [{
      width: Number(fixture.source_width),
      height: Number(fixture.source_height),
      rgba: Buffer.from(fixture.source_rgba_hex, 'hex'),
      vertices: [
        { x: -1, y: 1, u: 0, v: 0 },
        { x: 1, y: 1, u: 1, v: 0 },
        { x: -1, y: -1, u: 0, v: 1 },
        { x: 1, y: -1, u: 1, v: 1 },
      ],
      indices: [0, 1, 2, 2, 1, 3],
      samplingFilter: binding.TextureSamplingFilter.Nearest,
    }],
  };
}

async function collect() {
  if (typeof global.gc !== 'function') throw new Error('Node stress requires --expose-gc');
  global.gc();
  await new Promise((resolve) => setTimeout(resolve, 20));
  global.gc();
}

async function main() {
  const payload = Buffer.alloc(64 * 1024);
  for (let index = 0; index < 500; ++index) {
    const output = await binding.copyFrame(payload);
    assert.equal(output.length, payload.length);
    if ((index + 1) % 100 === 0) await collect();
  }
  await collect();
  const baseline = process.memoryUsage();

  for (let index = 0; index < 2000; ++index) {
    const output = await binding.copyFrame(payload);
    assert.equal(output.length, payload.length);
    if ((index + 1) % 100 === 0) await collect();
  }
  for (let index = 0; index < 500; ++index) {
    const timer = binding.runAfter(10_000, () => {
      throw new Error('cancelled timer callback was delivered');
    });
    timer.cancel();
    timer.close();
  }
  if (binding.runtimeInfo().components.tmr) {
    for (let index = 0; index < 500; ++index) {
      const camera = binding._createWebCamera();
      assert.equal(camera.isOpen(), false);
      camera.close();
    }
  }
  if (binding.runtimeInfo().components.oui) {
    const request = projectionRequest();
    const expected = Buffer.from(fixture.expected_rgba_hex, 'hex');
    for (let index = 0; index < 20; ++index) {
      const frame = await binding.renderProjection(request);
      assert.deepEqual(frame.data, expected);
    }
  }

  await collect();
  const finalMemory = process.memoryUsage();
  const rssGrowth = finalMemory.rss - baseline.rss;
  const externalGrowth = finalMemory.external - baseline.external;
  const arrayBufferGrowth = finalMemory.arrayBuffers - baseline.arrayBuffers;
  const rssLimit = 256 * 1024 * 1024;
  const externalLimit = 32 * 1024 * 1024;
  assert.ok(rssGrowth <= rssLimit,
    `Node native resident growth exceeded ${rssLimit}: ${rssGrowth}`);
  assert.ok(externalGrowth <= externalLimit,
    `Node external memory growth exceeded ${externalLimit}: ${externalGrowth}`);
  assert.ok(arrayBufferGrowth <= externalLimit,
    `Node ArrayBuffer growth exceeded ${externalLimit}: ${arrayBufferGrowth}`);
  console.log(`Node stress growth: rss=${rssGrowth}, external=${externalGrowth}, `
    + `arrayBuffers=${arrayBufferGrowth} bytes`);
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
