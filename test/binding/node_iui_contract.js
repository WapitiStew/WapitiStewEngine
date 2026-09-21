'use strict';

// IUI keyboard contract for the WSE Node-API binding.
// Hardware-free: a machine without a readable keyboard must report that through the readiness
// state and a structured error, never through a crash or a silently empty snapshot.

const assert = require('node:assert/strict');
const binding = require(process.argv[2]);

const groups = {
  ascii: binding.KeyboardGroupSize.ascii,
  function: binding.KeyboardGroupSize.function,
  arrow: binding.KeyboardGroupSize.arrow,
  lock: binding.KeyboardGroupSize.lock,
  command: binding.KeyboardGroupSize.command,
};

function observe(operation) {
  try { return operation(); }
  catch (error) {
    // Match one error's own state/category, not a preceding readiness observation.
    const categories = new Map([[0, 3], [2, 2], [3, 8], [4, 4]]);
    assert.ok(categories.has(error.code), 'known failing keyboard state');
    assert.equal(error.category, categories.get(error.code));
    return undefined;
  }
}

function main() {
  assert.equal(binding.runtimeInfo().components.iui, true);
  assert.deepEqual(groups, { ascii: 128, function: 24, arrow: 4, lock: 3, command: 9 });

  const keyboard = binding._createKeyboard();
  try {
    const state = keyboard.accessState();
    assert.ok(Object.values(binding.KeyboardAccessState).includes(state));
    assert.equal(typeof keyboard.isAvailable(), 'boolean');

    // Startup/hotplug can change availability between any two calls.
    const snapshot = observe(() => keyboard.snapshot());
    if (snapshot !== undefined) {
      assert.deepEqual(Object.keys(snapshot).sort(), Object.keys(groups).sort());
      for (const [name, count] of Object.entries(groups)) {
        assert.equal(snapshot[name].length, count, name);
        assert.ok(snapshot[name].every((value) => typeof value === 'boolean'), name);
      }
    }
    const ascii = observe(() => keyboard.pressedAscii());
    if (ascii !== undefined) assert.equal(typeof ascii, 'number');
  } finally {
    keyboard.close();
  }

  assert.equal(keyboard.isClosed(), true);
  keyboard.close();
  assert.throws(() => keyboard.snapshot(), { code: 'WSE_KEYBOARD_CLOSED' });

  console.log('node IUI contract passed');
}

main();
