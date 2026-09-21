//*****************************************************************************************************************
//!
//! @file    CameraTypes.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Tmr Cameraの列挙型と値型.
//! @brief   \~english  Tmr camera enumerations and value types.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

namespace WapitiStew.Wse;

/// <summary>Camera backend selection.</summary>
public enum CameraBackend
{
    /// <summary>Let Tmr pick the backend supported by this platform.</summary>
    Automatic = 0,
    /// <summary>Media Foundation, available on Windows.</summary>
    MediaFoundation = 1,
    /// <summary>Video4Linux2, available on Linux.</summary>
    Video4Linux2 = 2,
    /// <summary>libcamera, available on supported Linux devices.</summary>
    Libcamera = 3,
}

/// <summary>Pixel format of a camera stream or frame.</summary>
public enum CameraPixelFormat
{
    /// <summary>The format could not be identified.</summary>
    Unknown = 0,
    /// <summary>8-bit grayscale.</summary>
    Gray8 = 1,
    /// <summary>8-bit RGB.</summary>
    Rgb8 = 2,
    /// <summary>8-bit BGR.</summary>
    Bgr8 = 3,
    /// <summary>8-bit BGRA.</summary>
    Bgra8 = 4,
    /// <summary>Packed YUYV 4:2:2.</summary>
    Yuyv422 = 5,
    /// <summary>Planar NV12.</summary>
    Nv12 = 6,
    /// <summary>Motion JPEG.</summary>
    Mjpeg = 7,
    /// <summary>16-bit grayscale.</summary>
    Gray16 = 8,
    /// <summary>16-bit RGB.</summary>
    Rgb16 = 9,
    /// <summary>16-bit BGR.</summary>
    Bgr16 = 10,
    /// <summary>16-bit Bayer; the name gives the top-left two-by-two block.</summary>
    Bayer16Rggb = 11,
    /// <summary>16-bit Bayer; the name gives the top-left two-by-two block.</summary>
    Bayer16Bggr = 12,
    /// <summary>16-bit Bayer; the name gives the top-left two-by-two block.</summary>
    Bayer16Grbg = 13,
    /// <summary>16-bit Bayer; the name gives the top-left two-by-two block.</summary>
    Bayer16Gbrg = 14,
    /// <summary>Packed 4:2:2 with U0 Y0 V0 Y1 byte order.</summary>
    Uyvy422 = 15,
}

/// <summary>An orientation correction applied to a frame.</summary>
public enum ImageOrientation
{
    /// <summary>No change.</summary>
    None = 0,
    /// <summary>90 degrees clockwise.</summary>
    Rotate90Cw = 1,
    /// <summary>180 degrees.</summary>
    Rotate180 = 2,
    /// <summary>90 degrees counter-clockwise.</summary>
    Rotate90Ccw = 3,
    /// <summary>Mirrored left to right.</summary>
    FlipHorizontal = 4,
    /// <summary>Mirrored top to bottom.</summary>
    FlipVertical = 5,
}

/// <summary>The Bayer layout of a frame's top-left two-by-two block.</summary>
public enum BayerPattern
{
    /// <summary>R, Gr / Gb, B from the top left.</summary>
    Rggb = 0,
    /// <summary>B, Gb / Gr, R from the top left.</summary>
    Bggr = 1,
    /// <summary>Gr, R / B, Gb from the top left.</summary>
    Grbg = 2,
    /// <summary>Gb, B / R, Gr from the top left.</summary>
    Gbrg = 3,
}

/// <summary>How a Bayer frame is raised to colour.</summary>
public enum DemosaicMethod
{
    /// <summary>
    /// Copies a block's four samples onto its four pixels, so no sample value changes.
    /// </summary>
    Block2x2 = 0,
    /// <summary>Averages each pixel's neighbours, so edges come out smoother.</summary>
    Bilinear = 1,
}

/// <summary>How the camera is attached to the machine.</summary>
public enum CameraTransport
{
    /// <summary>The transport could not be identified.</summary>
    Unknown = 0,
    /// <summary>USB Video Class.</summary>
    UsbUvc = 1,
    /// <summary>Camera Serial Interface.</summary>
    Csi = 2,
    /// <summary>A virtual or software camera.</summary>
    Virtual = 3,
    /// <summary>A network camera.</summary>
    Network = 4,
}

/// <summary>A controllable camera parameter.</summary>
public enum CameraControl
{
    /// <summary>Exposure time.</summary>
    Exposure = 0,
    /// <summary>Sensor gain.</summary>
    Gain = 1,
    /// <summary>Focus position.</summary>
    Focus = 2,
    /// <summary>Brightness.</summary>
    Brightness = 3,
    /// <summary>Contrast.</summary>
    Contrast = 4,
    /// <summary>Saturation.</summary>
    Saturation = 5,
    /// <summary>White balance.</summary>
    WhiteBalance = 6,
    /// <summary>Zoom.</summary>
    Zoom = 7,
    /// <summary>Iris aperture.</summary>
    Iris = 8,
    /// <summary>Hue.</summary>
    Hue = 9,
    /// <summary>Sharpness.</summary>
    Sharpness = 10,
    /// <summary>Gamma.</summary>
    Gamma = 11,
    /// <summary>Color enable.</summary>
    ColorEnable = 12,
    /// <summary>Backlight compensation.</summary>
    BacklightCompensation = 13,
    /// <summary>Pan.</summary>
    Pan = 14,
    /// <summary>Tilt.</summary>
    Tilt = 15,
    /// <summary>Roll.</summary>
    Roll = 16,
    /// <summary>Power line frequency.</summary>
    PowerLineFrequency = 17,
    /// <summary>Capture frame rate.</summary>
    FrameRate = 18,
}

/// <summary>Whether a control is driven manually or by the device.</summary>
public enum CameraControlMode
{
    /// <summary>The caller sets the value.</summary>
    Manual = 0,
    /// <summary>The device chooses the value.</summary>
    Automatic = 1,
}

/// <summary>Physical unit a control value maps onto.</summary>
public enum CameraControlUnit
{
    /// <summary>An opaque device-native value with no physical unit.</summary>
    DeviceNative = 0,
    /// <summary>Microseconds.</summary>
    Microseconds = 1,
    /// <summary>Kelvin.</summary>
    Kelvin = 2,
    /// <summary>Diopters.</summary>
    Diopters = 3,
    /// <summary>A gain multiplier.</summary>
    GainMultiplier = 4,
    /// <summary>A relative step.</summary>
    Relative = 5,
    /// <summary>Degrees.</summary>
    Degrees = 6,
    /// <summary>Hertz.</summary>
    Hertz = 7,
    /// <summary>A boolean flag.</summary>
    Boolean = 8,
}

/// <summary>One camera stream format.</summary>
/// <param name="Width">Frame width in pixels.</param>
/// <param name="Height">Frame height in pixels.</param>
/// <param name="FrameRateNumerator">Frame-rate numerator.</param>
/// <param name="FrameRateDenominator">Frame-rate denominator.</param>
/// <param name="PixelFormat">Pixel format produced by this stream.</param>
public readonly record struct CameraFormat(
    int Width,
    int Height,
    int FrameRateNumerator,
    int FrameRateDenominator,
    CameraPixelFormat PixelFormat)
{
    /// <summary>Gets the frame rate in frames per second, or zero when it is unknown.</summary>
    public double FramesPerSecond =>
        FrameRateDenominator == 0 ? 0.0 : (double)FrameRateNumerator / FrameRateDenominator;

    internal static CameraFormat FromNative(NativeCameraFormat value) => new(
        (int)value.Width,
        (int)value.Height,
        (int)value.FrameRateNumerator,
        (int)value.FrameRateDenominator,
        (CameraPixelFormat)value.PixelFormat);
}

/// <summary>How a camera stream is opened and which output format the caller wants.</summary>
/// <param name="NativeFormat">Stream format the device produces.</param>
/// <param name="OutputFormat">Pixel format the caller wants frames in.</param>
/// <param name="AllowConversion">Whether Tmr may convert to the output format.</param>
public readonly record struct CameraStreamConfiguration(
    CameraFormat NativeFormat,
    CameraPixelFormat OutputFormat,
    bool AllowConversion)
{
    internal NativeCameraStreamConfiguration ToNative() => new()
    {
        NativeFormat = new NativeCameraFormat
        {
            Width = (uint)NativeFormat.Width,
            Height = (uint)NativeFormat.Height,
            FrameRateNumerator = (uint)NativeFormat.FrameRateNumerator,
            FrameRateDenominator = (uint)NativeFormat.FrameRateDenominator,
            PixelFormat = (int)NativeFormat.PixelFormat,
        },
        OutputFormat = (int)OutputFormat,
        AllowConversion = AllowConversion ? 1 : 0,
    };
}

/// <summary>The current value of one camera control.</summary>
/// <param name="Control">Control the value belongs to.</param>
/// <param name="Mode">Whether the value is manual or device-chosen.</param>
/// <param name="Value">Device-native value.</param>
public readonly record struct CameraControlValue(
    CameraControl Control,
    CameraControlMode Mode,
    long Value);

/// <summary>What one camera control supports.</summary>
/// <param name="Control">Control being described.</param>
/// <param name="Minimum">Smallest accepted device-native value.</param>
/// <param name="Maximum">Largest accepted device-native value.</param>
/// <param name="Step">Granularity between accepted values.</param>
/// <param name="DefaultValue">Device default value.</param>
/// <param name="SupportsManual">Whether the caller may set the value.</param>
/// <param name="SupportsAutomatic">Whether the device may choose the value.</param>
/// <param name="Unit">Physical unit the value maps onto.</param>
/// <param name="PhysicalScale">Multiplier from device-native value to physical unit.</param>
/// <param name="Readable">Whether the value can be read.</param>
/// <param name="Writable">Whether the value can be written.</param>
/// <param name="DisplayName">Human-readable control name reported by the device.</param>
public readonly record struct CameraControlCapability(
    CameraControl Control,
    long Minimum,
    long Maximum,
    long Step,
    long DefaultValue,
    bool SupportsManual,
    bool SupportsAutomatic,
    CameraControlUnit Unit,
    double PhysicalScale,
    bool Readable,
    bool Writable,
    string DisplayName)
{
    internal static CameraControlCapability FromNative(
        NativeCameraControlCapability value, string displayName) => new(
        (CameraControl)value.Control,
        value.Minimum,
        value.Maximum,
        value.Step,
        value.DefaultValue,
        value.SupportsManual != 0,
        value.SupportsAutomatic != 0,
        (CameraControlUnit)value.Unit,
        value.PhysicalScale,
        value.Readable != 0,
        value.Writable != 0,
        displayName);
}

/// <summary>USB identity of a camera.</summary>
/// <param name="VendorId">USB vendor identifier.</param>
/// <param name="ProductId">USB product identifier.</param>
/// <param name="UvcVersionBcd">UVC version in binary-coded decimal.</param>
/// <param name="SerialNumber">USB serial number, or an empty string.</param>
/// <param name="Available">Whether the device reported a usable USB identity.</param>
public readonly record struct CameraUsbIdentity(
    int VendorId,
    int ProductId,
    int UvcVersionBcd,
    string SerialNumber,
    bool Available);

/// <summary>Dimensions and pixel layout of one captured frame.</summary>
/// <param name="Width">Frame width in pixels.</param>
/// <param name="Height">Frame height in pixels.</param>
/// <param name="PixelFormat">Pixel format of the frame bytes.</param>
/// <param name="RowStride">Number of bytes per row.</param>
public readonly record struct CameraFrameDescription(
    int Width,
    int Height,
    CameraPixelFormat PixelFormat,
    long RowStride);

/// <summary>One stream profile: a native format and the output formats it can produce.</summary>
/// <param name="NativeFormat">Format the device produces.</param>
/// <param name="OutputFormats">Formats Tmr can deliver from it.</param>
public readonly record struct CameraStreamProfile(
    CameraFormat NativeFormat,
    System.Collections.Generic.IReadOnlyList<CameraPixelFormat> OutputFormats);
