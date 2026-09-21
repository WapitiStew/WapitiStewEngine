'use strict';

// Handle-free OUI projection: render one warped RGBA frame without touching a graphics API.
//
// Hardware-free by design: the sample asks for the software adapter, so it runs on machines with
// no discrete GPU. Build with WSE_BUILD_OUI=ON and WSE_BUILD_NODE_BINDING=ON.
//
// The argument is either the installed `lang/js` package directory or a built `wse.node` addon.
// Given the addon the sample still loads the package, so what is exercised is the public
// JavaScript surface an application uses rather than the raw native entry points.
//
// The whole OUI handle API is deliberately absent from this binding. Where the C++ counterpart
// initializes a Renderer and then creates, uploads, fences, reads back, and destroys a texture, a
// mesh, and a surface by handle, JavaScript gets one call: renderProjection() performs that entire
// pipeline and hands back the finished frame. Nothing to release, and nothing to close.
//
//   node example/js/oui/projection.js <installed lang/js directory or wse.node>
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

if ( wse.renderProjection === undefined ) {
  throw new Error('This WSE build does not include the OUI component.');
}

async function main() {
  // A 2x2 RGBA source: red, green, blue, white.
  const source = Buffer.from([
    255, 0, 0, 255, 0, 255, 0, 255,
    0, 0, 255, 255, 255, 255, 255, 255,
  ]);

  // The mesh maps the source texture onto the output quad in normalized device coordinates.
  // `width` and `height` describe the source image, not the output; `rgba` must hold exactly
  // width * height * 4 packed bytes. The two triangles named by `indices` cover the whole quad,
  // x and y running -1..1 across the target and u and v running 0..1 across the source. Bending
  // this mesh is what "projection" means here: move a vertex and the image warps with it.
  // `opacity`, `edgeBlend`, and a separate `alpha` mask are optional and left at their defaults.
  const layer = {
    width: 2,
    height: 2,
    rgba: source,
    vertices: [
      { x: -1, y: 1, u: 0, v: 0 },
      { x: 1, y: 1, u: 1, v: 0 },
      { x: -1, y: -1, u: 0, v: 1 },
      { x: 1, y: -1, u: 1, v: 1 },
    ],
    indices: [0, 1, 2, 2, 1, 3],
    // Nearest keeps the 2x2 source as four flat quadrants when it is scaled up to the 4x4 target,
    // which makes the result readable pixel by pixel below. Linear would blend the quadrants and
    // the top-left pixel would no longer be pure red.
    samplingFilter: wse.TextureSamplingFilter.Nearest,
  };

  // renderProjection() is asynchronous for a reason: the render runs on a libuv worker thread and
  // the promise resolves on the loop, so a long render never blocks the main thread the way the
  // synchronous XPT calls do. The request is fully read and its buffers copied before the work is
  // queued, so `source` may be reused or dropped the moment this call returns.
  const frame = await wse.renderProjection({
    // The output size is independent of the source size; 4x4 is simply the smallest target that
    // shows the 2x2 source scaled. `supersampleScale` would render larger and downsample, and
    // `clearColor` would set what shows through where no triangle covers the target.
    outputWidth: 4,
    outputHeight: 4,
    // Naming the backend keeps the sample deterministic per platform. RendererBackend.Automatic
    // would let OUI pick, and `adapterName` would demand a particular adapter by substring, where
    // no match is a failure rather than a quiet render somewhere else.
    backend: process.platform === 'win32'
      ? wse.RendererBackend.Direct3D12
      : wse.RendererBackend.Vulkan12,
    // Software rendering keeps the sample runnable on machines without a discrete GPU.
    // Omitting it, or setting it false, asks for a hardware adapter and fails where none exists.
    // `timeoutMilliseconds` and `enableValidation` are left at their defaults.
    useSoftwareAdapter: true,
    layers: [layer],
  });

  // Renderer, surface, texture, mesh, and readback objects stay owned inside OUI; only the
  // packed RGBA8 result frame crosses the language boundary. They are all destroyed before the
  // call returns, so there is no handle to leak and nothing for a finally block to protect. The
  // frame's `data` is a Buffer owned by JavaScript and collected like any other.
  console.log('frame size:', frame.width, 'x', frame.height);
  // The readback may pad each row, so rowPitch can exceed width * 4. Walk the image by rowPitch,
  // never by width * 4, or the rows will shear. `frame.adapterName`, not printed here, names the
  // adapter that actually drew the frame, which is the only report of that choice when the
  // request did not name one.
  console.log('row pitch:', frame.rowPitch);
  console.log('byte count:', frame.data.length);
  console.log('top-left pixel RGBA:', [...frame.data.subarray(0, 4)]);
}

main().catch((error) => {
  console.error(error.category, error.code, error.nativeCode, error.message);
  process.exitCode = 1;
});
