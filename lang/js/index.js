'use strict';

const path = require('node:path');
const fs = require('node:fs');

function resolveAddon() {
  if ( process.env.WSE_NODE_ADDON ) {
    return path.resolve(process.env.WSE_NODE_ADDON);
  }
  const candidates = [
    path.join(__dirname, 'wse.node'),
    path.join(__dirname, '..', '..', 'bin', 'wse.node'),
  ];
  const candidate = candidates.find((value) => fs.existsSync(value));
  if ( !candidate ) {
    throw new Error(
      'WSE Node-API addon was not found. Build/install WSE with WSE_BUILD_NODE_BINDING=ON.',
    );
  }
  return candidate;
}

const native = require(resolveAddon());

class WebCamera {
  constructor() {
    this._native = native._createWebCamera();
  }

  static enumerate(backend = native.CameraBackend.Automatic) {
    return native._webCameraDevices(backend);
  }

  static capabilities(device) {
    return native._webCameraCapabilities(device);
  }

  open(device, configuration) {
    if ( configuration === undefined ) this._native.open(device);
    else this._native.open(device, configuration);
  }

  start(callback) {
    if ( callback === undefined ) this._native.start();
    else this._native.start(callback);
  }
  stop() { this._native.stop(); }
  readFrame(timeoutMilliseconds) { return this._native.readFrame(timeoutMilliseconds); }
  currentCapabilities() { return this._native.currentCapabilities(); }
  controlCapability(control) { return this._native.controlCapability(control); }
  getControl(control) { return this._native.getControl(control); }
  setControl(value) { this._native.setControl(value); }
  getExtensionUnit(selector) { return this._native.getExtensionUnit(selector); }
  setExtensionUnit(value) { this._native.setExtensionUnit(value); }
  isOpen() { return this._native.isOpen(); }
  isStreaming() { return this._native.isStreaming(); }
  // A session close: streaming stops and the device closes, but the object stays usable, so
  // open() may be called again with another stream profile. Only collection is terminal.
  close() { this._native.close(); }
}

class Keyboard {
  constructor() {
    if ( native._createKeyboard === undefined ) {
      throw new Error('This WSE build does not include the IUI component.');
    }
    this._native = native._createKeyboard();
  }

  accessState() { return this._native.accessState(); }
  isAvailable() { return this._native.isAvailable(); }
  snapshot() { return this._native.snapshot(); }
  pressedAscii() { return this._native.pressedAscii(); }
  isClosed() { return this._native.isClosed(); }
  close() { this._native.close(); }
}

// XPT operations block the calling thread until they finish or their explicit deadline elapses.
// Every operation therefore requires a finite `timeoutMs`; keep it short on the main thread, or run
// these calls in a Worker when a long deadline is needed.
class CancellationSource {
  constructor() {
    if ( native._createCancellationSource === undefined ) {
      throw new Error('This WSE build does not include the XPT component.');
    }
    this._native = native._createCancellationSource();
  }

  cancel() { this._native.cancel(); }
  isCancellationRequested() { return this._native.isCancellationRequested(); }
}

function nativeContext(context) {
  if ( context === null || typeof context !== 'object' ) {
    throw new TypeError('An operation context requires a timeoutMs property.');
  }
  const cancellation = context.cancellation instanceof CancellationSource
    ? context.cancellation._native
    : undefined;
  return { timeoutMs: context.timeoutMs, cancellation };
}

class TcpClient {
  constructor() {
    if ( native._createTcpClient === undefined ) {
      throw new Error('This WSE build does not include the XPT component.');
    }
    this._native = native._createTcpClient();
  }

  connect(host, port, context) { this._native.connect(host, port, nativeContext(context)); }
  disconnect() { this._native.disconnect(); }
  isConnected() { return this._native.isConnected(); }
  checkPeerConnection() { this._native.checkPeerConnection(); }
  remoteEndpoint() { return this._native.remoteEndpoint(); }
  localEndpoint() { return this._native.localEndpoint(); }
  send(data, context) { return this._native.send(data, nativeContext(context)); }
  receive(maximumSize, context) { return this._native.receive(maximumSize, nativeContext(context)); }
}

class UdpClient {
  constructor() {
    if ( native._createUdpClient === undefined ) {
      throw new Error('This WSE build does not include the XPT component.');
    }
    this._native = native._createUdpClient();
  }

  static maximumDatagramSize() { return native._udpMaximumDatagramSize(); }

  bind(host, port, context) { this._native.bind(host, port, nativeContext(context)); }
  close() { this._native.close(); }
  isOpen() { return this._native.isOpen(); }
  localEndpoint() { return this._native.localEndpoint(); }
  sendTo(host, port, data, context) {
    return this._native.sendTo(host, port, data, nativeContext(context));
  }
  receiveFrom(maximumSize, context) {
    return this._native.receiveFrom(maximumSize, nativeContext(context));
  }
}

class SerialPort {
  constructor() {
    if ( native._createSerialPort === undefined ) {
      throw new Error('This WSE build does not include the XPT component.');
    }
    this._native = native._createSerialPort();
  }

  open(deviceName, baudRate, context) {
    this._native.open(deviceName, baudRate, nativeContext(context));
  }
  close() { this._native.close(); }
  isOpen() { return this._native.isOpen(); }
  deviceName() { return this._native.deviceName(); }
  baudRate() { return this._native.baudRate(); }
  send(data, context) { return this._native.send(data, nativeContext(context)); }
  receive(maximumSize, context) { return this._native.receive(maximumSize, nativeContext(context)); }
}

// A 4xx or 5xx answer still throws, but the thrown Error carries the received response on its
// `response` property when `error.code === TransportErrorCode.HttpStatusError`.
function httpExecute(request, maximumResponseBodySize, context, credentials) {
  if ( native.httpExecute === undefined ) {
    throw new Error('This WSE build does not include the XPT component.');
  }
  if ( credentials === undefined ) {
    return native.httpExecute(request, maximumResponseBodySize, nativeContext(context));
  }
  return native.httpExecute(
    request, maximumResponseBodySize, nativeContext(context),
    credentials.username, credentials.secret,
  );
}

class ModelProfile {
  constructor(nativeProfile) {
    this._native = nativeProfile;
  }

  static supportedSchemaVersion() {
    requireVpj();
    return native._modelProfileSupportedSchemaVersion();
  }

  static fromJson(jsonText) {
    requireVpj();
    return new ModelProfile(native._modelProfileFromJson(jsonText));
  }

  static fromFile(filePath) {
    requireVpj();
    return new ModelProfile(native._modelProfileFromFile(filePath));
  }

  describe() { return this._native.describe(); }
}

class ModelProfileRegistry {
  constructor() {
    requireVpj();
    this._native = native._createModelProfileRegistry();
  }

  registerProfile(profile, replaceExisting = false) {
    this._native.registerProfile(profile._native, replaceExisting);
  }
  loadFile(filePath, replaceExisting = false) {
    this._native.loadFile(filePath, replaceExisting);
  }
  loadDirectory(directoryPath, replaceExisting = false) {
    return this._native.loadDirectory(directoryPath, replaceExisting);
  }
  find(modelOrAliasId) { return new ModelProfile(this._native.find(modelOrAliasId)); }
  resolveIdentity(modelOrAliasId) { return this._native.resolveIdentity(modelOrAliasId); }
  profileIds() { return this._native.profileIds(); }
}

class ProfileControlSession {
  constructor() {
    requireVpj();
    this._native = native._createProfileControlSession();
  }

  configure(profile, protocolId, endpoint) {
    this._native.configure(profile._native, protocolId, endpoint);
  }
  isConfigured() { return this._native.isConfigured(); }
  connect(context) { this._native.connect(context); }
  disconnect() { this._native.disconnect(); }
  isConnected() { return this._native.isConnected(); }
  execute(commandId, requestBody, context) {
    return this._native.execute(commandId, requestBody, context);
  }
}

function requireVpj() {
  if ( native._createModelProfileRegistry === undefined ) {
    throw new Error('This WSE build does not include the projector control component.');
  }
}

// The addon defines its entry points with `napi_default`, which makes them non-enumerable, so an
// object spread would silently drop every native function. Copy the own property names instead and
// keep the leading-underscore names internal to this module.
const publicNative = {};
for (const name of Object.getOwnPropertyNames(native)) {
  if ( name.startsWith('_') ) continue;
  publicNative[name] = native[name];
}

// Moving or averaging pixels is image work, not device work, so these are free functions rather
// than methods on a camera. Each takes and returns the same plain frame object a read gives back.
const isFrameOperationSupported = (pixelFormat) => native._isFrameOperationSupported(pixelFormat);
const isFrameAveragingSupported = (pixelFormat) => native._isFrameAveragingSupported(pixelFormat);
const isBayerFormat = (pixelFormat) => native._isBayerFormat(pixelFormat);
const bayerPatternOf = (pixelFormat) => native._bayerPatternOf(pixelFormat);
const applyOrientation = (frame, orientation) => native._applyOrientation(frame, orientation);
const demosaicFrame = (frame, outputFormat, method) =>
  method === undefined
    ? native._demosaicFrame(frame, outputFormat)
    : native._demosaicFrame(frame, outputFormat, method);
const averageFrames = (frames) => native._averageFrames(frames);

module.exports = {
  ...publicNative,
  isFrameOperationSupported,
  isFrameAveragingSupported,
  isBayerFormat,
  bayerPatternOf,
  applyOrientation,
  demosaicFrame,
  averageFrames,
  WebCamera,
  Keyboard,
  CancellationSource,
  TcpClient,
  UdpClient,
  SerialPort,
  httpExecute,
  ModelProfile,
  ModelProfileRegistry,
  ProfileControlSession,
};
