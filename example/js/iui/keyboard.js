'use strict';

// Portable IUI keyboard readiness check and one consistent key snapshot.
//
// Hardware-free by design: a machine without a readable keyboard reports that through the
// readiness state instead of failing. Build with WSE_BUILD_IUI=ON and WSE_BUILD_NODE_BINDING=ON.
//
// The argument is either the installed `lang/js` package directory or a built `wse.node` addon.
// Given the addon the sample still loads the package, so what is exercised is the public
// JavaScript surface an application uses rather than the raw native entry points.
//
// Two things the binding does not carry: the C++ counterpart reports through
// wse::registDefaultLog() and wse::WLog(), which have no JavaScript equivalent, so this sample
// writes to the console; and a Keyboard offers no event or callback surface at all, so readiness
// is polled and the key state is sampled.
//
//   node example/js/iui/keyboard.js <installed lang/js directory or wse.node>
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

if ( wse.Keyboard === undefined ) {
  throw new Error('This WSE build does not include the IUI component.');
}

// Every WSE enumeration reaches JavaScript as a plain object of number-valued properties, not as a
// type, so a readable report needs the reverse map built here.
const stateNames = Object.fromEntries(
  Object.entries(wse.KeyboardAccessState).map(([name, value]) => [value, name]),
);

// A group arrives as a fixed-length array of booleans, one entry per key; wse.KeyboardGroupSize
// carries the length of each group for a caller that wants to index it by key rather than count.
function countPressed(group) {
  return group.reduce((total, pressed) => total + (pressed ? 1 : 0), 0);
}

async function main() {
  // Construction starts the backend's monitoring, and this object is its only owner. JavaScript
  // has no deterministic destructor, so the try/finally below is what releases it: skip close()
  // and the monitor keeps running until the garbage collector finalizes the object, at an
  // unpredictable moment or, in a short-lived script, not at all before exit.
  const keyboard = new wse.Keyboard();
  try {
    // The backend needs a moment before it reports its first reading.
    // Twenty attempts of 25 ms is a half-second ceiling, generous for every backend measured. A
    // shorter budget would report Starting on a slow machine and skip the snapshot; a longer one
    // only delays the same answer where the keyboard is genuinely unavailable. wse.wait() is the
    // binding's own timer: it resolves a promise, so awaiting it yields the loop rather than
    // blocking it.
    for ( let attempt = 0; attempt < 20 && !keyboard.isAvailable(); ++attempt ) {
      await wse.wait(25);
    }

    const state = keyboard.accessState();
    console.log('keyboard state:', stateNames[state] ?? state);

    if ( !keyboard.isAvailable() ) {
      // Not a failure of this sample: snapshot() would throw a WseError explaining why.
      // Unavailable, PermissionDenied, and Disconnected all arrive here, and each read re-checks
      // the readiness state before touching the device, so nothing partial is ever returned.
      // Returning normally is deliberate; a machine with no readable keyboard is a supported
      // environment for this sample, and the process still exits with success.
      try {
        keyboard.snapshot();
      } catch (error) {
        console.log('keyboard is not readable:', error.message);
      }
      return;
    }

    // One snapshot is one consistent point in time; do not combine several separate reads.
    const snapshot = keyboard.snapshot();
    console.log('pressed ascii keys:', countPressed(snapshot.ascii));
    console.log('pressed function keys:', countPressed(snapshot.function));
    console.log('pressed arrow keys:', countPressed(snapshot.arrow));
    console.log('pressed lock keys:', countPressed(snapshot.lock));
    console.log('pressed command keys:', countPressed(snapshot.command));
    console.log('pressed ascii code:', keyboard.pressedAscii());
  } finally {
    // close() releases the native monitor and is safe to call twice, but it is terminal for a
    // Keyboard: unlike WebCamera.close(), which only ends a session, every later call on this
    // object throws "The keyboard is already closed.". Create a new Keyboard to read again.
    keyboard.close();
  }
}

// A failure raised by WSE carries the structured triple printed here. A plain TypeError from the
// binding's argument checks does not, and shows as undefined in the first three fields.
main().catch((error) => {
  console.error(error.category, error.code, error.nativeCode, error.message);
  process.exitCode = 1;
});
