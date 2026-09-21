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

function fnv1a64(data) {
  let value = 14695981039346656037n;
  for (const byte of data) {
    value = BigInt.asUintN(64, (value ^ BigInt(byte)) * 1099511628211n);
  }
  return value;
}

async function main() {
  assert.equal(binding.runtimeInfo().components.oui, true);
  assert.equal(binding.RendererBackend.Direct3D12, 1);
  assert.equal(binding.RendererBackend.Vulkan12, 2);
  assert.equal(binding.TextureSamplingFilter.Nearest, 0);
  assert.equal(binding.EdgeBlendCurve.Smoothstep, 1);
  const request = {
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
  const frame = await binding.renderProjection(request);
  const expected = Buffer.from(fixture.expected_rgba_hex, 'hex');
  assert.equal(frame.width, request.outputWidth);
  assert.equal(frame.height, request.outputHeight);
  assert.equal(frame.rowPitch, request.outputWidth * 4);
  // Which adapter drew the frame is reported whether or not one was named on the request.
  assert.equal(typeof frame.adapterName, "string");
  assert.ok(frame.adapterName.length > 0, "the adapter that drew the frame must be named");
  // Naming an adapter while asking for the software adapter is two different requests, so it is
  // refused rather than quietly resolved one way.
  await assert.rejects(binding.renderProjection(
    Object.assign({}, request, { adapterName: "no-such-adapter-anywhere" })));
  // A named adapter that no machine has fails rather than drawing on some other one.
  await assert.rejects(binding.renderProjection(Object.assign({}, request, {
    useSoftwareAdapter: false, adapterName: "no-such-adapter-anywhere" })));
  assert.ok(Buffer.isBuffer(frame.data));
  assert.deepEqual(frame.data, expected);
  assert.equal(fnv1a64(frame.data), BigInt(`0x${fixture.expected_fnv1a64}`));

  await assert.rejects(binding.renderProjection({ outputWidth: 0, outputHeight: 0, layers: [] }),
    (error) => Number.isInteger(error.category) && error.category !== 0
      && Number.isInteger(error.code) && error.code !== 0);
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
