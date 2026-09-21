from enum import Enum
from types import TracebackType
from typing import Callable, overload, Sequence, TypedDict
from _typeshed import ReadableBuffer

class Components(TypedDict):
    xpt: bool
    tmr: bool
    oui: bool
    gef: bool
    iui: bool
    vpj: bool

class RuntimeInfo(TypedDict):
    version: str
    binding_abi_version: int
    components: Components

class ErrorCategory(Enum):
    NONE: ErrorCategory
    INVALID_ARGUMENT: ErrorCategory
    NOT_FOUND: ErrorCategory
    INVALID_STATE: ErrorCategory
    INPUT_OUTPUT: ErrorCategory
    TIMEOUT: ErrorCategory
    CANCELLATION: ErrorCategory
    PROTOCOL: ErrorCategory
    SECURITY: ErrorCategory
    UNSUPPORTED: ErrorCategory
    RESOURCE_EXHAUSTED: ErrorCategory
    INTERNAL: ErrorCategory

class WseError(RuntimeError):
    category: int
    code: int
    native_code: int

class FrameBuffer:
    @property
    def size(self) -> int: ...
    def tobytes(self) -> bytes: ...
    def __bytes__(self) -> bytes: ...
    def __len__(self) -> int: ...

class Runtime:
    def __init__(self) -> None: ...
    def info(self) -> RuntimeInfo: ...
    def copy_frame(self, buffer: ReadableBuffer) -> FrameBuffer: ...
    def wait(self, milliseconds: int) -> None: ...
    def close(self) -> None: ...
    @property
    def closed(self) -> bool: ...
    def __enter__(self) -> Runtime: ...
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool: ...

def runtime_info() -> RuntimeInfo: ...
def copy_frame(buffer: ReadableBuffer) -> FrameBuffer: ...
def wait(milliseconds: int) -> None: ...

class CameraBackend(Enum):
    AUTOMATIC: CameraBackend
    MEDIA_FOUNDATION: CameraBackend
    VIDEO4LINUX2: CameraBackend
    LIBCAMERA: CameraBackend

class CameraPixelFormat(Enum):
    UNKNOWN: CameraPixelFormat
    GRAY8: CameraPixelFormat
    RGB8: CameraPixelFormat
    BGR8: CameraPixelFormat
    BGRA8: CameraPixelFormat
    YUYV422: CameraPixelFormat
    UYVY422: CameraPixelFormat
    NV12: CameraPixelFormat
    MJPEG: CameraPixelFormat
    GRAY16: CameraPixelFormat
    RGB16: CameraPixelFormat
    BGR16: CameraPixelFormat
    BAYER16_RGGB: CameraPixelFormat
    BAYER16_BGGR: CameraPixelFormat
    BAYER16_GRBG: CameraPixelFormat
    BAYER16_GBRG: CameraPixelFormat

class ImageOrientation(Enum):
    NONE: ImageOrientation
    ROTATE_90_CW: ImageOrientation
    ROTATE_180: ImageOrientation
    ROTATE_90_CCW: ImageOrientation
    FLIP_HORIZONTAL: ImageOrientation
    FLIP_VERTICAL: ImageOrientation

class BayerPattern(Enum):
    RGGB: BayerPattern
    BGGR: BayerPattern
    GRBG: BayerPattern
    GBRG: BayerPattern

class DemosaicMethod(Enum):
    BLOCK_2X2: DemosaicMethod
    BILINEAR: DemosaicMethod

class CameraControl(Enum):
    EXPOSURE: CameraControl
    GAIN: CameraControl
    FOCUS: CameraControl
    BRIGHTNESS: CameraControl
    CONTRAST: CameraControl
    SATURATION: CameraControl
    WHITE_BALANCE: CameraControl
    ZOOM: CameraControl
    IRIS: CameraControl
    HUE: CameraControl
    SHARPNESS: CameraControl
    GAMMA: CameraControl
    COLOR_ENABLE: CameraControl
    BACKLIGHT_COMPENSATION: CameraControl
    PAN: CameraControl
    TILT: CameraControl
    ROLL: CameraControl
    POWER_LINE_FREQUENCY: CameraControl
    FRAME_RATE: CameraControl

class CameraControlMode(Enum):
    MANUAL: CameraControlMode
    AUTOMATIC: CameraControlMode

class CameraControlUnit(Enum):
    DEVICE_NATIVE: CameraControlUnit
    MICROSECONDS: CameraControlUnit
    KELVIN: CameraControlUnit
    DIOPTERS: CameraControlUnit
    GAIN_MULTIPLIER: CameraControlUnit
    RELATIVE: CameraControlUnit
    DEGREES: CameraControlUnit
    HERTZ: CameraControlUnit
    BOOLEAN: CameraControlUnit

class CameraTransport(Enum):
    UNKNOWN: CameraTransport
    USB_UVC: CameraTransport
    CSI: CameraTransport
    VIRTUAL: CameraTransport
    NETWORK: CameraTransport

class CameraUsbIdentity:
    def __init__(self) -> None: ...
    vendor_id: int
    product_id: int
    serial_number: str
    uvc_version_bcd: int
    @property
    def available(self) -> bool: ...

class CameraDevice:
    def __init__(self) -> None: ...
    backend: CameraBackend
    id: str
    display_name: str
    transport: str
    transport_type: CameraTransport
    usb: CameraUsbIdentity
    @property
    def valid(self) -> bool: ...

class CameraFormat:
    def __init__(self) -> None: ...
    width: int
    height: int
    frame_rate_numerator: int
    frame_rate_denominator: int
    pixel_format: CameraPixelFormat
    @property
    def frames_per_second(self) -> float: ...
    @property
    def valid(self) -> bool: ...

class CameraControlCapability:
    def __init__(self) -> None: ...
    control: CameraControl
    minimum: int
    maximum: int
    step: int
    default_value: int
    supports_manual: bool
    supports_automatic: bool
    unit: CameraControlUnit
    physical_scale: float
    readable: bool
    writable: bool
    display_name: str
    def value_from_normalized(self, normalized: float) -> int: ...
    def normalized_from_value(self, value: int) -> float: ...
    def physical_from_value(self, value: int) -> float: ...
    def value_from_physical(self, physical: float) -> int: ...

class CameraControlValue:
    def __init__(self) -> None: ...
    control: CameraControl
    mode: CameraControlMode
    value: int

class CameraStreamProfile:
    def __init__(self) -> None: ...
    native_format: CameraFormat
    output_formats: list[CameraPixelFormat]
    @property
    def valid(self) -> bool: ...
    def supports_output(self, output: CameraPixelFormat) -> bool: ...

class CameraStreamConfiguration:
    def __init__(self) -> None: ...
    native_format: CameraFormat
    output_format: CameraPixelFormat
    allow_conversion: bool
    @property
    def valid(self) -> bool: ...

class CameraExtensionUnitSelector:
    def __init__(self) -> None: ...
    unit_guid: bytes
    unit_id: int
    selector: int
    minimum_size: int
    maximum_size: int
    readable: bool
    writable: bool
    display_name: str
    @property
    def valid(self) -> bool: ...

class CameraExtensionUnitValue:
    def __init__(self) -> None: ...
    selector: CameraExtensionUnitSelector
    payload: bytes
    @property
    def valid(self) -> bool: ...

class CameraCapability:
    device: CameraDevice
    formats: list[CameraFormat]
    controls: list[CameraControlCapability]
    stream_profiles: list[CameraStreamProfile]
    extension_units: list[CameraExtensionUnitSelector]

class CameraFrame:
    def __init__(self, width: int, height: int, pixel_format: CameraPixelFormat,
                 data: ReadableBuffer, row_stride: int = 0, sequence: int = 0,
                 monotonic_timestamp_ns: int = 0) -> None: ...
    width: int
    height: int
    pixel_format: CameraPixelFormat
    row_stride: int
    sequence: int
    monotonic_timestamp_ns: int
    data: FrameBuffer

class CameraFrameAccumulator:
    def __init__(self) -> None: ...
    def add(self, frame: CameraFrame) -> None: ...
    def average(self) -> CameraFrame: ...
    def reset(self) -> None: ...
    @property
    def count(self) -> int: ...

def is_frame_operation_supported(pixel_format: CameraPixelFormat) -> bool: ...
def is_frame_averaging_supported(pixel_format: CameraPixelFormat) -> bool: ...
def is_bayer_format(pixel_format: CameraPixelFormat) -> bool: ...
def bayer_pattern_of(pixel_format: CameraPixelFormat) -> BayerPattern: ...
def apply_orientation(frame: CameraFrame, orientation: ImageOrientation) -> CameraFrame: ...
def demosaic_frame(frame: CameraFrame, output_format: CameraPixelFormat,
                   method: DemosaicMethod = DemosaicMethod.BILINEAR) -> CameraFrame: ...

class WebCamera:
    def __init__(self) -> None: ...
    @staticmethod
    def enumerate(backend: CameraBackend = CameraBackend.AUTOMATIC) -> list[CameraDevice]: ...
    @staticmethod
    def capabilities(device: CameraDevice) -> CameraCapability: ...
    @overload
    def open(self, device: CameraDevice) -> None: ...
    @overload
    def open(self, device: CameraDevice, configuration: CameraStreamConfiguration) -> None: ...
    def start(self, callback: Callable[[CameraFrame | WseError], None] | None = None) -> None: ...
    def stop(self) -> None: ...
    def read_frame(self, timeout_ms: int) -> CameraFrame: ...
    def current_capabilities(self) -> CameraCapability: ...
    def control_capability(self, control: CameraControl) -> CameraControlCapability: ...
    def get_control(self, control: CameraControl) -> CameraControlValue: ...
    def set_control(self, value: CameraControlValue) -> None: ...
    def get_extension_unit(self, selector: CameraExtensionUnitSelector) -> CameraExtensionUnitValue: ...
    def set_extension_unit(self, value: CameraExtensionUnitValue) -> None: ...
    def close(self) -> None: ...
    def release(self) -> None: ...
    @property
    def released(self) -> bool: ...
    @property
    def is_open(self) -> bool: ...
    @property
    def is_streaming(self) -> bool: ...
    def __enter__(self) -> WebCamera: ...
    def __exit__(self, exc_type: type[BaseException] | None,
                 exc_value: BaseException | None,
                 traceback: TracebackType | None) -> bool: ...

class TransportErrorCode(Enum):
    NONE: TransportErrorCode
    INVALID_ARGUMENT: TransportErrorCode
    HOST_NOT_FOUND: TransportErrorCode
    ADDRESS_UNAVAILABLE: TransportErrorCode
    CONNECTION_REFUSED: TransportErrorCode
    CONNECTION_RESET: TransportErrorCode
    NETWORK_UNREACHABLE: TransportErrorCode
    NOT_CONNECTED: TransportErrorCode
    REMOTE_CLOSED: TransportErrorCode
    TIMED_OUT: TransportErrorCode
    CANCELLED: TransportErrorCode
    BIND_FAILED: TransportErrorCode
    SEND_FAILED: TransportErrorCode
    RECEIVE_FAILED: TransportErrorCode
    MESSAGE_TOO_LARGE: TransportErrorCode
    DATAGRAM_TRUNCATED: TransportErrorCode
    RESOURCE_EXHAUSTED: TransportErrorCode
    UNSUPPORTED: TransportErrorCode
    UNKNOWN: TransportErrorCode
    OPEN_FAILED: TransportErrorCode
    CONFIGURATION_FAILED: TransportErrorCode
    HTTP_STATUS_ERROR: TransportErrorCode
    RESPONSE_TOO_LARGE: TransportErrorCode
    SECURITY_FAILED: TransportErrorCode

class HttpMethod(Enum):
    GET: HttpMethod
    HEAD: HttpMethod
    POST: HttpMethod
    PUT: HttpMethod
    PATCH: HttpMethod
    DELETE: HttpMethod

class CancellationSource:
    def __init__(self) -> None: ...
    def cancel(self) -> None: ...
    @property
    def is_cancellation_requested(self) -> bool: ...

class OperationContext:
    def __init__(self, timeout_ms: int, cancellation: CancellationSource | None = None) -> None: ...
    @property
    def timeout_ms(self) -> int: ...

class TcpClient:
    def __init__(self) -> None: ...
    def connect(self, host: str, port: int, context: OperationContext) -> None: ...
    def disconnect(self) -> None: ...
    def check_peer_connection(self) -> None: ...
    def remote_endpoint(self) -> tuple[str, int]: ...
    def local_endpoint(self) -> tuple[str, int]: ...
    def send(self, data: ReadableBuffer, context: OperationContext) -> int: ...
    def receive(self, maximum_size: int, context: OperationContext) -> bytes: ...
    @property
    def is_connected(self) -> bool: ...
    def __enter__(self) -> TcpClient: ...
    def __exit__(self, exc_type: type[BaseException] | None,
                 exc_value: BaseException | None,
                 traceback: TracebackType | None) -> bool: ...

class UdpClient:
    def __init__(self) -> None: ...
    @staticmethod
    def maximum_datagram_size() -> int: ...
    def bind(self, host: str, port: int, context: OperationContext) -> None: ...
    def close(self) -> None: ...
    def local_endpoint(self) -> tuple[str, int]: ...
    def send_to(self, host: str, port: int, data: ReadableBuffer,
                context: OperationContext) -> int: ...
    def receive_from(self, maximum_size: int,
                     context: OperationContext) -> tuple[str, int, bytes]: ...
    @property
    def is_open(self) -> bool: ...
    def __enter__(self) -> UdpClient: ...
    def __exit__(self, exc_type: type[BaseException] | None,
                 exc_value: BaseException | None,
                 traceback: TracebackType | None) -> bool: ...

class SerialPort:
    def __init__(self) -> None: ...
    def open(self, device_name: str, baud_rate: int, context: OperationContext) -> None: ...
    def close(self) -> None: ...
    def send(self, data: ReadableBuffer, context: OperationContext) -> int: ...
    def receive(self, maximum_size: int, context: OperationContext) -> bytes: ...
    @property
    def is_open(self) -> bool: ...
    @property
    def device_name(self) -> str: ...
    @property
    def baud_rate(self) -> int: ...
    def __enter__(self) -> SerialPort: ...
    def __exit__(self, exc_type: type[BaseException] | None,
                 exc_value: BaseException | None,
                 traceback: TracebackType | None) -> bool: ...

class HttpRequest:
    def __init__(self, method: HttpMethod, url: str) -> None: ...
    def add_header(self, name: str, value: str) -> None: ...
    def set_body(self, body: ReadableBuffer) -> None: ...

class HttpResponse:
    @property
    def status_code(self) -> int: ...
    @property
    def attempt_count(self) -> int: ...
    def headers(self) -> list[tuple[str, str]]: ...
    def body(self) -> bytes: ...

class WseTransferError(WseError):
    """Failed send progress or a detached truncated UDP prefix; never an automatic retry signal."""
    bytes_transferred: int
    received_data: bytes | None
    source_endpoint: tuple[str, int] | None

class WseHttpStatusError(WseError):
    """Raised when the server answered with a 4xx or 5xx.

    The exchange finished, so unlike every other failure this one carries the answer.
    """

    response: HttpResponse

def http_execute(request: HttpRequest, maximum_response_body_size: int,
                 context: OperationContext) -> HttpResponse: ...
def http_execute_authenticated(request: HttpRequest, maximum_response_body_size: int,
                               username: str, secret: str,
                               context: OperationContext) -> HttpResponse: ...

class RendererBackend(Enum):
    AUTOMATIC: RendererBackend
    DIRECT3D12: RendererBackend
    VULKAN12: RendererBackend

class TextureSamplingFilter(Enum):
    NEAREST: TextureSamplingFilter
    LINEAR: TextureSamplingFilter

class EdgeBlendCurve(Enum):
    LINEAR: EdgeBlendCurve
    SMOOTHSTEP: EdgeBlendCurve

class ProjectionVertex:
    def __init__(self, x: float, y: float, u: float, v: float) -> None: ...
    x: float
    y: float
    u: float
    v: float

class RendererColor:
    def __init__(self, red: float = 0.0, green: float = 0.0,
                 blue: float = 0.0, alpha: float = 1.0) -> None: ...
    red: float
    green: float
    blue: float
    alpha: float

class EdgeBlend:
    def __init__(self) -> None: ...
    left: float
    right: float
    top: float
    bottom: float
    curve: EdgeBlendCurve

class ProjectionLayer:
    def __init__(self, width: int, height: int, rgba: ReadableBuffer,
                 vertices: Sequence[ProjectionVertex], indices: Sequence[int],
                 alpha: ReadableBuffer | None = None) -> None: ...
    width: int
    height: int
    sampling_filter: TextureSamplingFilter
    opacity: float
    edge_blend: EdgeBlend

class ProjectionRequest:
    def __init__(self) -> None: ...
    output_width: int
    output_height: int
    clear_color: RendererColor
    supersample_scale: int
    backend: RendererBackend
    use_software_adapter: bool
    enable_validation: bool
    timeout_ms: int
    layers: list[ProjectionLayer]

class ProjectionFrame:
    width: int
    height: int
    row_pitch: int
    data: FrameBuffer

def render_projection(request: ProjectionRequest) -> ProjectionFrame: ...

KEYBOARD_ASCII_COUNT: int
KEYBOARD_FUNCTION_COUNT: int
KEYBOARD_ARROW_COUNT: int
KEYBOARD_LOCK_COUNT: int
KEYBOARD_COMMAND_COUNT: int

class KeyboardAccessState(Enum):
    STARTING = 0
    READY = 1
    UNAVAILABLE = 2
    PERMISSION_DENIED = 3
    DISCONNECTED = 4

class KeyboardSnapshot(TypedDict):
    ascii: list[bool]
    function: list[bool]
    arrow: list[bool]
    lock: list[bool]
    command: list[bool]

class Keyboard:
    def __init__(self) -> None: ...
    @property
    def access_state(self) -> KeyboardAccessState: ...
    @property
    def is_available(self) -> bool: ...
    @property
    def closed(self) -> bool: ...
    def snapshot(self) -> KeyboardSnapshot: ...
    def pressed_ascii(self) -> int: ...
    def close(self) -> None: ...
    def __enter__(self) -> Keyboard: ...
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: object | None,
    ) -> bool: ...
