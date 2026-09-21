'use strict';

// WSE JavaScript quick start: load the binding and report what this build actually carries.
//
// Every JS sample takes the same first argument, and this one demonstrates it on its own. Two
// forms are accepted: the installed `lang/js` package directory, or a built `wse.node` addon.
// WSE_JAVASCRIPT_PACKAGE supplies the same value from the environment when no argument is given.
// Unlike a sample's own optional arguments, this one is required: without it there is nothing to
// demonstrate, so a missing package is a hard error rather than a quiet success.
//
//   node example/js/core/quickstart.js <installed lang/js directory or wse.node>

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
// runtimeInfo() is the one call every build answers, before any component is touched. It reports
// the WSE semantic version, the binding ABI number the addon was compiled against, and a
// `components` flag per component. Those flags are the same question each other sample asks in a
// narrower form when it tests `wse.WebCamera === undefined` or `wse.httpExecute === undefined`.
const info = wse.runtimeInfo();
// The version is asserted rather than merely printed: a malformed version means the addon and the
// package are not the pair the installer produced, which is worth failing on here rather than
// somewhere deep inside a component sample.
if ( !/^\d+\.\d+\.\d+$/.test(info.version) ) throw new Error('Invalid WSE semantic version.');
console.log(info);
