export interface RuntimeInfo {
  readonly version: string;
  readonly bindingAbiVersion: number;
  readonly components: {
    readonly xpt: boolean;
    readonly tmr: boolean;
    readonly oui: boolean;
    readonly gef: boolean;
    readonly iui: boolean;
    readonly vpj: boolean;
  };
}

export interface WseError extends Error {
  readonly category: number;
  readonly code: number;
  readonly nativeCode: number;
  /**
   * The response that was received despite the failure. Present only when `code` is
   * `TransportErrorCode.HttpStatusError`, which is how `httpExecute` reports a 4xx or 5xx
   * answer: the call still throws, but the status code, headers, and body are readable here.
   * `undefined` for every other failure.
   */
  readonly response?: HttpResponse;
}

/** A failed send or truncated UDP receive; catchable as the existing Error/WseError. */
export interface TransferError extends WseError {
  /** Completed send count or UDP prefix length. Does not prove peer acceptance. */
  readonly bytesTransferred: number;
  /** Detached bytes, present only for DatagramTruncated. The discarded suffix is lost. */
  readonly receivedData?: Buffer;
  readonly sourceEndpoint?: Endpoint;
}

export interface TimerHandle {
  cancel(): void;
  close(): void;
}

export enum CameraBackend {
  Automatic = 0,
  MediaFoundation = 1,
  Video4Linux2 = 2,
  Libcamera = 3,
}

export enum CameraPixelFormat {
  Unknown = 0,
  Gray8 = 1,
  Rgb8 = 2,
  Bgr8 = 3,
  Bgra8 = 4,
  Yuyv422 = 5,
  Nv12 = 6,
  Mjpeg = 7,
  Gray16 = 8,
  Rgb16 = 9,
  Bgr16 = 10,
  Bayer16Rggb = 11,
  Bayer16Bggr = 12,
  Bayer16Grbg = 13,
  Bayer16Gbrg = 14,
  Uyvy422 = 15,
}

export enum ImageOrientation {
  None = 0,
  Rotate90Cw = 1,
  Rotate180 = 2,
  Rotate90Ccw = 3,
  FlipHorizontal = 4,
  FlipVertical = 5,
}

export enum BayerPattern {
  Rggb = 0,
  Bggr = 1,
  Grbg = 2,
  Gbrg = 3,
}

export enum DemosaicMethod {
  Block2x2 = 0,
  Bilinear = 1,
}

export enum CameraControl {
  Exposure = 0,
  Gain = 1,
  Focus = 2,
  Brightness = 3,
  Contrast = 4,
  Saturation = 5,
  WhiteBalance = 6,
  Zoom = 7,
  Iris = 8,
  Hue = 9,
  Sharpness = 10,
  Gamma = 11,
  ColorEnable = 12,
  BacklightCompensation = 13,
  Pan = 14,
  Tilt = 15,
  Roll = 16,
  PowerLineFrequency = 17,
  FrameRate = 18,
}

export enum CameraControlMode {
  Manual = 0,
  Automatic = 1,
}

export enum CameraControlUnit {
  DeviceNative = 0,
  Microseconds = 1,
  Kelvin = 2,
  Diopters = 3,
  GainMultiplier = 4,
  Relative = 5,
  Degrees = 6,
  Hertz = 7,
  Boolean = 8,
}

export enum CameraTransport {
  Unknown = 0,
  UsbUvc = 1,
  Csi = 2,
  Virtual = 3,
  Network = 4,
}

export interface CameraUsbIdentity {
  readonly vendorId: number;
  readonly productId: number;
  readonly serialNumber: string;
  readonly uvcVersionBcd: number;
  readonly available: boolean;
}

export interface CameraDevice {
  readonly backend: CameraBackend;
  readonly id: string;
  readonly displayName: string;
  readonly transport: string;
  readonly transportType: CameraTransport;
  readonly usb: CameraUsbIdentity;
}

export interface CameraFormat {
  readonly width: number;
  readonly height: number;
  readonly frameRateNumerator: number;
  readonly frameRateDenominator: number;
  readonly pixelFormat: CameraPixelFormat;
  readonly framesPerSecond: number;
}

export interface CameraControlCapability {
  readonly control: CameraControl;
  readonly minimum: number;
  readonly maximum: number;
  readonly step: number;
  readonly defaultValue: number;
  readonly supportsManual: boolean;
  readonly supportsAutomatic: boolean;
  readonly unit: CameraControlUnit;
  readonly physicalScale: number;
  readonly readable: boolean;
  readonly writable: boolean;
  readonly displayName: string;
}

export interface CameraStreamProfile {
  readonly nativeFormat: CameraFormat;
  readonly outputFormats: readonly CameraPixelFormat[];
}

export interface CameraStreamConfiguration {
  readonly nativeFormat: CameraFormat;
  readonly outputFormat: CameraPixelFormat;
  readonly allowConversion?: boolean;
}

export interface CameraExtensionUnitSelector {
  readonly unitGuid: Buffer;
  readonly unitId: number;
  readonly selector: number;
  readonly minimumSize: number;
  readonly maximumSize: number;
  readonly readable: boolean;
  readonly writable: boolean;
  readonly displayName: string;
}

export interface CameraExtensionUnitValue {
  readonly selector: CameraExtensionUnitSelector;
  readonly payload: Buffer;
}

export interface CameraCapability {
  readonly device: CameraDevice;
  readonly formats: readonly CameraFormat[];
  readonly controls: readonly CameraControlCapability[];
  readonly streamProfiles: readonly CameraStreamProfile[];
  readonly extensionUnits: readonly CameraExtensionUnitSelector[];
}

export interface CameraControlValue {
  readonly control: CameraControl;
  readonly mode: CameraControlMode;
  readonly value: number;
}

export interface CameraFrame {
  readonly width: number;
  readonly height: number;
  readonly pixelFormat: CameraPixelFormat;
  readonly rowStride: number;
  readonly sequence: number;
  readonly monotonicTimestampNs: number;
  readonly data: Buffer;
}

/** Whether a pixel format can be reoriented. A Bayer format cannot. */
export function isFrameOperationSupported(pixelFormat: CameraPixelFormat): boolean;
/** Whether a pixel format can be averaged. A Bayer format can. */
export function isFrameAveragingSupported(pixelFormat: CameraPixelFormat): boolean;
export function isBayerFormat(pixelFormat: CameraPixelFormat): boolean;
/** Throws for a format that carries no Bayer layout. */
export function bayerPatternOf(pixelFormat: CameraPixelFormat): BayerPattern;
/** A quarter turn exchanges the width and the height. A Bayer frame is refused. */
export function applyOrientation(frame: CameraFrame, orientation: ImageOrientation): CameraFrame;
/** The result format is one of Rgb8, Bgr8, Rgb16, and Bgr16. */
export function demosaicFrame(
  frame: CameraFrame,
  outputFormat: CameraPixelFormat,
  method?: DemosaicMethod,
): CameraFrame;
/** Averages frames of one shape, rounding to nearest. */
export function averageFrames(frames: readonly CameraFrame[]): CameraFrame;

export class WebCamera {
  constructor();
  static enumerate(backend?: CameraBackend): CameraDevice[];
  static capabilities(device: CameraDevice): CameraCapability;
  open(device: CameraDevice, configuration?: CameraStreamConfiguration): void;
  start(callback?: (error: WseError | null, frame: CameraFrame | null) => void): void;
  stop(): void;
  readFrame(timeoutMilliseconds: number): Promise<CameraFrame>;
  currentCapabilities(): CameraCapability;
  controlCapability(control: CameraControl): CameraControlCapability;
  getControl(control: CameraControl): CameraControlValue;
  setControl(value: CameraControlValue): void;
  getExtensionUnit(selector: CameraExtensionUnitSelector): CameraExtensionUnitValue;
  setExtensionUnit(value: CameraExtensionUnitValue): void;
  isOpen(): boolean;
  isStreaming(): boolean;
  /**
   * Stops streaming and closes the device, ending the session. The object stays usable, so
   * `open()` may be called again -- which is how the frame size, fixed at `open()`, is changed.
   * Calling it again is safe. The native camera is released when the object is collected.
   */
  close(): void;
}

export enum RendererBackend {
  Automatic = 0,
  Direct3D12 = 1,
  Vulkan12 = 2,
}

export enum TextureSamplingFilter {
  Nearest = 0,
  Linear = 1,
}

export enum EdgeBlendCurve {
  Linear = 0,
  Smoothstep = 1,
}

export interface ProjectionVertex {
  readonly x: number;
  readonly y: number;
  readonly u: number;
  readonly v: number;
}

export interface RendererColor {
  readonly red?: number;
  readonly green?: number;
  readonly blue?: number;
  readonly alpha?: number;
}

export interface EdgeBlend {
  readonly left?: number;
  readonly right?: number;
  readonly top?: number;
  readonly bottom?: number;
  readonly curve?: EdgeBlendCurve;
}

export interface ProjectionLayer {
  readonly width: number;
  readonly height: number;
  readonly rgba: Buffer;
  readonly alpha?: Buffer;
  readonly vertices: readonly ProjectionVertex[];
  readonly indices: readonly number[];
  readonly samplingFilter?: TextureSamplingFilter;
  readonly opacity?: number;
  readonly edgeBlend?: EdgeBlend;
}

export interface ProjectionRequest {
  readonly outputWidth: number;
  readonly outputHeight: number;
  readonly clearColor?: RendererColor;
  readonly supersampleScale?: number;
  readonly backend?: RendererBackend;
  readonly useSoftwareAdapter?: boolean;
  readonly enableValidation?: boolean;
  /**
   * Part of the name of the adapter to render on. Omitting it chooses automatically; no
   * match is a failure rather than a quiet render on another adapter.
   */
  readonly adapterName?: string;
  readonly timeoutMilliseconds?: number;
  readonly layers: readonly ProjectionLayer[];
}

export interface ProjectionFrame {
  readonly width: number;
  readonly height: number;
  readonly rowPitch: number;
  /**
   * The adapter that actually drew the frame. When no adapter was named on the request,
   * this is the only place the choice is reported.
   */
  readonly adapterName: string;
  readonly data: Buffer;
}

export function runtimeInfo(): RuntimeInfo;
export function copyFrame(source: Buffer | ArrayBufferView): Promise<Buffer>;
export function wait(milliseconds: number): Promise<void>;
export function runAfter(milliseconds: number, callback: () => void): TimerHandle;
export function renderProjection(request: ProjectionRequest): Promise<ProjectionFrame>;

export enum KeyboardAccessState {
  Starting = 0,
  Ready = 1,
  Unavailable = 2,
  PermissionDenied = 3,
  Disconnected = 4,
}

export interface KeyboardGroupSize {
  readonly ascii: number;
  readonly function: number;
  readonly arrow: number;
  readonly lock: number;
  readonly command: number;
}

/** One consistent keyboard snapshot; true means pressed. */
export interface KeyboardSnapshot {
  readonly ascii: readonly boolean[];
  readonly function: readonly boolean[];
  readonly arrow: readonly boolean[];
  readonly lock: readonly boolean[];
  readonly command: readonly boolean[];
}

export const KeyboardGroupSize: KeyboardGroupSize;

/**
 * Reads physical keyboard input through IUI. Construction starts monitoring, so create one
 * instance and close it when finished. `snapshot()` and `pressedAscii()` throw a `WseError`
 * while the backend is not readable.
 */
export class Keyboard {
  constructor();
  accessState(): KeyboardAccessState;
  isAvailable(): boolean;
  snapshot(): KeyboardSnapshot;
  pressedAscii(): number;
  isClosed(): boolean;
  close(): void;
}

export enum TransportErrorCode {
  None = 0,
  InvalidArgument = 1,
  HostNotFound = 2,
  AddressUnavailable = 3,
  ConnectionRefused = 4,
  ConnectionReset = 5,
  NetworkUnreachable = 6,
  NotConnected = 7,
  RemoteClosed = 8,
  TimedOut = 9,
  Cancelled = 10,
  BindFailed = 11,
  SendFailed = 12,
  ReceiveFailed = 13,
  MessageTooLarge = 14,
  DatagramTruncated = 15,
  ResourceExhausted = 16,
  Unsupported = 17,
  Unknown = 18,
  OpenFailed = 19,
  ConfigurationFailed = 20,
  HttpStatusError = 21,
  ResponseTooLarge = 22,
  SecurityFailed = 23,
}

export enum HttpMethod {
  Get = 0,
  Head = 1,
  Post = 2,
  Put = 3,
  Patch = 4,
  Delete = 5,
}

/** A host name or numeric address paired with a TCP or UDP port. */
export interface TransportEndpoint {
  readonly host: string;
  readonly port: number;
}

/**
 * Explicit controls required by every XPT operation. There is deliberately no default timeout.
 * XPT calls block the calling thread until they finish or `timeoutMs` elapses, so keep the
 * deadline short on the main thread or run the call in a Worker.
 */
export interface OperationContext {
  readonly timeoutMs: number;
  readonly cancellation?: CancellationSource;
}

/** Requests cooperative cancellation of an in-flight XPT operation. */
export class CancellationSource {
  constructor();
  cancel(): void;
  isCancellationRequested(): boolean;
}

/** A caller-confined TCP connection. */
export class TcpClient {
  constructor();
  connect(host: string, port: number, context: OperationContext): void;
  disconnect(): void;
  isConnected(): boolean;
  checkPeerConnection(): void;
  remoteEndpoint(): TransportEndpoint;
  localEndpoint(): TransportEndpoint;
  send(data: Buffer | ArrayBufferView, context: OperationContext): number;
  receive(maximumSize: number, context: OperationContext): Buffer;
}

/** One received UDP datagram with its source endpoint. */
export interface UdpDatagram {
  readonly source: TransportEndpoint;
  readonly payload: Buffer;
}

/** A UDP socket. */
export class UdpClient {
  constructor();
  static maximumDatagramSize(): number;
  bind(host: string, port: number, context: OperationContext): void;
  close(): void;
  isOpen(): boolean;
  localEndpoint(): TransportEndpoint;
  sendTo(
    host: string,
    port: number,
    data: Buffer | ArrayBufferView,
    context: OperationContext,
  ): number;
  receiveFrom(maximumSize: number, context: OperationContext): UdpDatagram;
}

/** A serial port. Only the portable new API is bound; the legacy connector is not exposed. */
export class SerialPort {
  constructor();
  open(deviceName: string, baudRate: number, context: OperationContext): void;
  close(): void;
  isOpen(): boolean;
  deviceName(): string;
  baudRate(): number;
  send(data: Buffer | ArrayBufferView, context: OperationContext): number;
  receive(maximumSize: number, context: OperationContext): Buffer;
}

export interface HttpHeader {
  readonly name: string;
  readonly value: string;
}

export interface HttpRequest {
  readonly method: HttpMethod;
  readonly url: string;
  readonly headers?: readonly HttpHeader[];
  readonly body?: Buffer | ArrayBufferView;
}

export interface HttpResponse {
  readonly statusCode: number;
  readonly attemptCount: number;
  readonly headers: readonly HttpHeader[];
  readonly body: Buffer;
}

export interface HttpCredentials {
  readonly username: string;
  readonly secret: string;
}

/**
 * Executes an HTTP request. The binding never retries on the caller's behalf: a request is treated
 * as non-idempotent and transient-failure retries stay off.
 *
 * A 4xx or 5xx answer is thrown as a `WseError` whose `code` is
 * `TransportErrorCode.HttpStatusError`; that error carries the received response on its
 * `response` property, so the status code is still readable.
 */
export function httpExecute(
  request: HttpRequest,
  maximumResponseBodySize: number,
  context: OperationContext,
  credentials?: HttpCredentials,
): HttpResponse;

export enum ModelProtocolKind {
  Serial = 0,
  NetworkV1 = 1,
  NetworkV2 = 2,
  WebApi = 3,
}

export enum ModelTransport {
  Serial = 0,
  Tcp = 1,
  Http = 2,
}

export enum ModelAuthentication {
  None = 0,
  MonitorPassword = 1,
  WebControlDigest = 2,
  RuntimeHttp = 3,
}

export enum ModelIdentityState {
  Known = 0,
  Unknown = 1,
}

export interface ModelProtocolDescription {
  readonly id: string;
  readonly kind: ModelProtocolKind;
  readonly transport: ModelTransport;
  readonly authentication: ModelAuthentication;
  readonly credentialReference: string;
}

export interface ModelCommandDescription {
  readonly id: string;
  readonly protocolId: string;
  readonly readOnly: boolean;
}

export interface ModelConstraints {
  readonly maximumResponseBytes: number;
  readonly operationTimeoutMs: number;
}

export interface ModelProfileDescription {
  readonly schemaVersion: number;
  readonly modelId: string;
  readonly displayName: string;
  readonly aliases: readonly string[];
  readonly capabilities: readonly string[];
  readonly protocols: readonly ModelProtocolDescription[];
  readonly commands: readonly ModelCommandDescription[];
  readonly constraints: ModelConstraints;
}

/** A validated model profile. Profile JSON is parsed natively and never exposed as a JSON type. */
export class ModelProfile {
  static supportedSchemaVersion(): number;
  static fromJson(jsonText: string): ModelProfile;
  static fromFile(filePath: string): ModelProfile;
  describe(): ModelProfileDescription;
}

export interface ModelIdentity {
  readonly state: ModelIdentityState;
  readonly profileId: string;
}

export interface ModelProfileBatchResult {
  readonly discovered: number;
  readonly registered: number;
}

/** A set of registered model profiles, looked up by model id or alias. */
export class ModelProfileRegistry {
  constructor();
  registerProfile(profile: ModelProfile, replaceExisting?: boolean): void;
  loadFile(filePath: string, replaceExisting?: boolean): void;
  loadDirectory(directoryPath: string, replaceExisting?: boolean): ModelProfileBatchResult;
  find(modelOrAliasId: string): ModelProfile;
  resolveIdentity(modelOrAliasId: string): ModelIdentity;
  profileIds(): string[];
}

/** Where a control session connects. */
export interface ProfileControlEndpoint {
  readonly address?: string;
  readonly port?: number;
  readonly baudRate?: number;
  readonly stableDeviceId?: string;
  readonly reconnectAttempts?: number;
  readonly reconnectDelayMs?: number;
}

export interface ProfileControlReply {
  readonly statusCode: number;
  readonly body: Buffer;
}

/**
 * Executes the commands a model profile declares. No credential provider is exposed, so
 * configuring a protocol that needs one is refused.
 */
export class ProfileControlSession {
  constructor();
  configure(profile: ModelProfile, protocolId: string, endpoint: ProfileControlEndpoint): void;
  isConfigured(): boolean;
  connect(context: OperationContext): void;
  disconnect(): void;
  isConnected(): boolean;
  execute(
    commandId: string,
    requestBody: Buffer | ArrayBufferView,
    context: OperationContext,
  ): ProfileControlReply;
}
