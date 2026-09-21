"""High-level Python API for WonderStewEngine."""

from __future__ import annotations

import os
from pathlib import Path

_dll_directory = None
if os.name == "nt" and hasattr(os, "add_dll_directory"):
    _package_root = Path(__file__).resolve().parents[3]
    _bin_directory = _package_root / "bin"
    if _bin_directory.is_dir():
        _dll_directory = os.add_dll_directory(str(_bin_directory))

from ._wse import (  # noqa: E402,F401
    ErrorCategory,
    FrameBuffer,
    Runtime,
    WseError,
    copy_frame,
    runtime_info,
    wait,
)
from . import _wse as _native  # noqa: E402

__all__ = [
    "ErrorCategory",
    "FrameBuffer",
    "Runtime",
    "WseError",
    "copy_frame",
    "runtime_info",
    "wait",
]

if hasattr(_native, "WebCamera"):
    CameraBackend = _native.CameraBackend
    CameraPixelFormat = _native.CameraPixelFormat
    CameraControl = _native.CameraControl
    CameraControlMode = _native.CameraControlMode
    CameraControlUnit = _native.CameraControlUnit
    CameraTransport = _native.CameraTransport
    CameraUsbIdentity = _native.CameraUsbIdentity
    CameraDevice = _native.CameraDevice
    CameraFormat = _native.CameraFormat
    CameraControlCapability = _native.CameraControlCapability
    CameraControlValue = _native.CameraControlValue
    CameraStreamProfile = _native.CameraStreamProfile
    CameraStreamConfiguration = _native.CameraStreamConfiguration
    CameraExtensionUnitSelector = _native.CameraExtensionUnitSelector
    CameraExtensionUnitValue = _native.CameraExtensionUnitValue
    CameraCapability = _native.CameraCapability
    CameraFrame = _native.CameraFrame
    ImageOrientation = _native.ImageOrientation
    BayerPattern = _native.BayerPattern
    DemosaicMethod = _native.DemosaicMethod
    CameraFrameAccumulator = _native.CameraFrameAccumulator
    is_frame_operation_supported = _native.is_frame_operation_supported
    is_frame_averaging_supported = _native.is_frame_averaging_supported
    is_bayer_format = _native.is_bayer_format
    bayer_pattern_of = _native.bayer_pattern_of
    apply_orientation = _native.apply_orientation
    demosaic_frame = _native.demosaic_frame
    WebCamera = _native.WebCamera
    __all__ += [
        "CameraBackend",
        "CameraPixelFormat",
        "CameraControl",
        "CameraControlMode",
        "CameraControlUnit",
        "CameraTransport",
        "CameraUsbIdentity",
        "CameraDevice",
        "CameraFormat",
        "CameraControlCapability",
        "CameraControlValue",
        "CameraStreamProfile",
        "CameraStreamConfiguration",
        "CameraExtensionUnitSelector",
        "CameraExtensionUnitValue",
        "CameraCapability",
        "CameraFrame",
        "ImageOrientation",
        "BayerPattern",
        "DemosaicMethod",
        "CameraFrameAccumulator",
        "is_frame_operation_supported",
        "is_frame_averaging_supported",
        "is_bayer_format",
        "bayer_pattern_of",
        "apply_orientation",
        "demosaic_frame",
        "WebCamera",
    ]

if hasattr(_native, "render_projection"):
    RendererBackend = _native.RendererBackend
    TextureSamplingFilter = _native.TextureSamplingFilter
    EdgeBlendCurve = _native.EdgeBlendCurve
    ProjectionVertex = _native.ProjectionVertex
    RendererColor = _native.RendererColor
    EdgeBlend = _native.EdgeBlend
    ProjectionLayer = _native.ProjectionLayer
    ProjectionRequest = _native.ProjectionRequest
    ProjectionFrame = _native.ProjectionFrame
    render_projection = _native.render_projection
    __all__ += [
        "RendererBackend",
        "TextureSamplingFilter",
        "EdgeBlendCurve",
        "ProjectionVertex",
        "RendererColor",
        "EdgeBlend",
        "ProjectionLayer",
        "ProjectionRequest",
        "ProjectionFrame",
        "render_projection",
    ]

if hasattr(_native, "Keyboard"):
    KeyboardAccessState = _native.KeyboardAccessState
    Keyboard = _native.Keyboard
    KEYBOARD_ASCII_COUNT = _native.KEYBOARD_ASCII_COUNT
    KEYBOARD_FUNCTION_COUNT = _native.KEYBOARD_FUNCTION_COUNT
    KEYBOARD_ARROW_COUNT = _native.KEYBOARD_ARROW_COUNT
    KEYBOARD_LOCK_COUNT = _native.KEYBOARD_LOCK_COUNT
    KEYBOARD_COMMAND_COUNT = _native.KEYBOARD_COMMAND_COUNT
    __all__ += [
        "KeyboardAccessState",
        "Keyboard",
        "KEYBOARD_ASCII_COUNT",
        "KEYBOARD_FUNCTION_COUNT",
        "KEYBOARD_ARROW_COUNT",
        "KEYBOARD_LOCK_COUNT",
        "KEYBOARD_COMMAND_COUNT",
    ]

if hasattr(_native, "UdpClient"):
    TransportErrorCode = _native.TransportErrorCode
    HttpMethod = _native.HttpMethod
    CancellationSource = _native.CancellationSource
    OperationContext = _native.OperationContext
    TcpClient = _native.TcpClient
    UdpClient = _native.UdpClient
    SerialPort = _native.SerialPort
    HttpRequest = _native.HttpRequest
    HttpResponse = _native.HttpResponse
    WseHttpStatusError = _native.WseHttpStatusError
    WseTransferError = _native.WseTransferError
    http_execute = _native.http_execute
    http_execute_authenticated = _native.http_execute_authenticated
    __all__ += [
        "TransportErrorCode",
        "HttpMethod",
        "CancellationSource",
        "OperationContext",
        "TcpClient",
        "UdpClient",
        "SerialPort",
        "HttpRequest",
        "HttpResponse",
        "WseHttpStatusError",
        "WseTransferError",
        "http_execute",
        "http_execute_authenticated",
    ]

if hasattr(_native, "ModelProfile"):
    ModelProtocolKind = _native.ModelProtocolKind
    ModelTransport = _native.ModelTransport
    ModelAuthentication = _native.ModelAuthentication
    ModelIdentityState = _native.ModelIdentityState
    ModelProfile = _native.ModelProfile
    ModelProfileRegistry = _native.ModelProfileRegistry
    ProfileControlSession = _native.ProfileControlSession
    __all__ += [
        "ModelProtocolKind",
        "ModelTransport",
        "ModelAuthentication",
        "ModelIdentityState",
        "ModelProfile",
        "ModelProfileRegistry",
        "ProfileControlSession",
    ]
