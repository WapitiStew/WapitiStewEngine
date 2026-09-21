//*****************************************************************************************************************
//!
//! @file    CameraTypes.h
//! @brief   \~japanese Backend非依存Tmr Camera ContractのData型を定義する.
//! @brief   \~english  Defines data types for the backend-independent Tmr camera contract.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-28, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_TMR_CAMERA_CAMERATYPES_H
#define WONDERSTEWENGINE_TMR_CAMERA_CAMERATYPES_H

#include <utility>
#include "../../dynamic.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wse
{
namespace tmr
{

enum class eCameraBackend : std::uint8_t
{
      Automatic       = 0U
    , MediaFoundation
    , Video4Linux2
    , Libcamera
};

enum class eCameraPixelFormat : std::uint8_t
{
      Unknown = 0U
    , Gray8
    , Rgb8
    , Bgr8
    , Bgra8
    , Yuyv422
    , Nv12
    , Mjpeg
    , Gray16       //!< \~japanese 16bit単色. \~english 16-bit monochrome.
    , Rgb16        //!< \~japanese 16bit RGB. \~english 16-bit RGB.
    , Bgr16        //!< \~japanese 16bit BGR. \~english 16-bit BGR.
    //! \~japanese 16bit Bayer. 名前は左上2x2の並びを表す.
    //! \~english  16-bit Bayer; the name gives the order of the top-left two-by-two block.
    , Bayer16Rggb
    , Bayer16Bggr
    , Bayer16Grbg
    , Bayer16Gbrg
    , Uyvy422      //!< \~japanese U0 Y0 V0 Y1順の4:2:2. \~english Packed 4:2:2 in U0 Y0 V0 Y1 order.
};

//! \~japanese Cameraの接続方式。取得不能な情報はUnknownのまま保持する.
//! \~english Camera transport. Information that cannot be determined remains Unknown.
enum class eCameraTransport : std::uint8_t
{
      Unknown = 0U
    , UsbUvc
    , Csi
    , Virtual
    , Network
};

enum class eCameraControl : std::uint8_t
{
      Exposure = 0U
    , Gain
    , Focus
    , Brightness
    , Contrast
    , Saturation
    , WhiteBalance
    , Zoom
    , Iris
    , Hue
    , Sharpness
    , Gamma
    , ColorEnable
    , BacklightCompensation
    , Pan
    , Tilt
    , Roll
    , PowerLineFrequency
    , FrameRate  //!< \~japanese 撮像Frame rate. \~english Capture frame rate.
};

enum class eCameraControlMode : std::uint8_t
{
      Manual = 0U
    , Automatic
};

//! \~japanese Control値を物理量へ変換するときの単位. DeviceNativeはBackend固有値を表す.
//! \~english Unit used to convert a control value to a physical quantity; DeviceNative is backend-specific.
enum class eCameraControlUnit : std::uint8_t
{
      DeviceNative = 0U
    , Microseconds
    , Kelvin
    , Diopters
    , GainMultiplier
    , Relative
    , Degrees
    , Hertz
    , Boolean
};

//! \~japanese 取得できたUSB/UVC識別情報。0または空文字列は未取得を表す.
//! \~english Available USB/UVC identity. Zero and an empty string mean unavailable.
struct sCameraUsbIdentity
{
    std::uint16_t vendor_id;
    std::uint16_t product_id;
    std::string   serial_number;
    std::uint16_t uvc_version_bcd;

    WSE_API bool available() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraUsbIdentity(
          std::uint16_t vendor_id_in = 0U
        , std::uint16_t product_id_in = 0U
        , const std::string& serial_number_in = {}
        , std::uint16_t uvc_version_bcd_in = 0U
    )
        : vendor_id       ( vendor_id_in )
        , product_id      ( product_id_in )
        , serial_number   ( serial_number_in )
        , uvc_version_bcd ( uvc_version_bcd_in )
    {
    }
};

struct sCameraDeviceInfo
{
    eCameraBackend backend;
    std::string    id;
    std::string    display_name;
    //! \~japanese 旧Source互換用の診断文字列。新規Codeはtransport_typeを使用する.
    //! \~english Diagnostic string retained for source compatibility; new code uses transport_type.
    std::string    transport;
    eCameraTransport transport_type;
    sCameraUsbIdentity usb;

    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraDeviceInfo(
          eCameraBackend backend_in = eCameraBackend::Automatic
        , const std::string& id_in = {}
        , const std::string& display_name_in = {}
        , const std::string& transport_in = {}
        , eCameraTransport transport_type_in = eCameraTransport::Unknown
        , const sCameraUsbIdentity& usb_in = {}
    )
        : backend        ( backend_in )
        , id             ( id_in )
        , display_name   ( display_name_in )
        , transport      ( transport_in )
        , transport_type ( transport_type_in )
        , usb            ( usb_in )
    {
    }
};

struct sCameraFormat
{
    std::uint32_t      width;
    std::uint32_t      height;
    std::uint32_t      frame_rate_numerator;
    std::uint32_t      frame_rate_denominator;
    eCameraPixelFormat pixel_format;

    WSE_API bool valid() const noexcept;
    WSE_API double framesPerSecond() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraFormat(
          std::uint32_t width_in = 0U
        , std::uint32_t height_in = 0U
        , std::uint32_t frame_rate_numerator_in = 0U
        , std::uint32_t frame_rate_denominator_in = 1U
        , eCameraPixelFormat pixel_format_in = eCameraPixelFormat::Unknown
    )
        : width                  ( width_in )
        , height                 ( height_in )
        , frame_rate_numerator   ( frame_rate_numerator_in )
        , frame_rate_denominator ( frame_rate_denominator_in )
        , pixel_format           ( pixel_format_in )
    {
    }
};

struct sCameraControlCapability
{
    eCameraControl control;
    std::int64_t   minimum;
    std::int64_t   maximum;
    std::int64_t   step;
    std::int64_t   default_value;
    bool           supports_manual;
    bool           supports_automatic;
    eCameraControlUnit unit;
    //! \~japanese `physical = value * physical_scale`。DeviceNativeでも1.0を使用する。
    //! \~english `physical = value * physical_scale`; DeviceNative also uses 1.0.
    double         physical_scale;
    bool           readable;
    bool           writable;
    std::string    display_name;

    WSE_API bool valid() const noexcept;
    //! \~japanese 0.0～1.0の共通位置をCapabilityのStepに丸めた値へ変換する.
    //! \~english Converts a common 0.0-to-1.0 position to a value aligned to the capability step.
    WSE_API std::int64_t valueFromNormalized( double normalized_in ) const noexcept;
    //! \~japanese Capability値を0.0～1.0の共通位置へ変換する.
    //! \~english Converts a capability value to a common 0.0-to-1.0 position.
    WSE_API double normalizedFromValue( std::int64_t value_in ) const noexcept;
    //! \~japanese Capability値を公開単位の物理量へ変換する.
    //! \~english Converts a capability value to its public physical unit.
    WSE_API double physicalFromValue( std::int64_t value_in ) const noexcept;
    //! \~japanese 公開単位の物理量をRange／Stepに丸めたCapability値へ変換する.
    //! \~english Converts a physical quantity to a range- and step-aligned capability value.
    WSE_API std::int64_t valueFromPhysical( double physical_in ) const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraControlCapability(
          eCameraControl control_in = eCameraControl::Exposure
        , std::int64_t minimum_in = 0
        , std::int64_t maximum_in = 0
        , std::int64_t step_in = 0
        , std::int64_t default_value_in = 0
        , bool supports_manual_in = false
        , bool supports_automatic_in = false
        , eCameraControlUnit unit_in = eCameraControlUnit::DeviceNative
        , double physical_scale_in = 1.0
        , bool readable_in = true
        , bool writable_in = true
        , const std::string& display_name_in = {}
    )
        : control            ( control_in )
        , minimum            ( minimum_in )
        , maximum            ( maximum_in )
        , step               ( step_in )
        , default_value      ( default_value_in )
        , supports_manual    ( supports_manual_in )
        , supports_automatic ( supports_automatic_in )
        , unit               ( unit_in )
        , physical_scale     ( physical_scale_in )
        , readable           ( readable_in )
        , writable           ( writable_in )
        , display_name       ( display_name_in )
    {
    }
};

//! \~japanese DeviceがNativeで出力するProfileと、変換可能な出力Format.
//! \~english A device-native stream profile and output formats available through conversion.
struct sCameraStreamProfile
{
    sCameraFormat native_format;
    std::vector< eCameraPixelFormat > output_formats;

    WSE_API bool valid() const noexcept;
    WSE_API bool supportsOutput( eCameraPixelFormat format_in ) const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraStreamProfile(
          const sCameraFormat& native_format_in = {}
        , const std::vector< eCameraPixelFormat >& output_formats_in = {}
    )
        : native_format  ( native_format_in )
        , output_formats ( output_formats_in )
    {
    }
};

//! \~japanese Native captureとConsumer出力を分離したStream要求.
//! \~english Stream request separating native capture from consumer output.
struct sCameraStreamConfiguration
{
    sCameraFormat      native_format;
    eCameraPixelFormat output_format;
    bool               allow_conversion;

    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraStreamConfiguration(
          const sCameraFormat& native_format_in = {}
        , eCameraPixelFormat output_format_in = eCameraPixelFormat::Unknown
        , bool allow_conversion_in = true
    )
        : native_format    ( native_format_in )
        , output_format    ( output_format_in )
        , allow_conversion ( allow_conversion_in )
    {
    }
};

//! \~japanese Generic UVC Extension Unit selector. OS固有型は含めない.
//! \~english Generic UVC extension-unit selector without operating-system types.
struct sCameraExtensionUnitSelector
{
    std::array< std::uint8_t, 16U > unit_guid;
    std::uint8_t unit_id;
    std::uint8_t selector;
    std::size_t minimum_size;
    std::size_t maximum_size;
    bool        readable;
    bool        writable;
    std::string display_name;

    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraExtensionUnitSelector(
          const std::array< std::uint8_t, 16U >& unit_guid_in = {}
        , std::uint8_t unit_id_in = 0U
        , std::uint8_t selector_in = 0U
        , std::size_t minimum_size_in = 0U
        , std::size_t maximum_size_in = 0U
        , bool readable_in = false
        , bool writable_in = false
        , const std::string& display_name_in = {}
    )
        : unit_guid    ( unit_guid_in )
        , unit_id      ( unit_id_in )
        , selector     ( selector_in )
        , minimum_size ( minimum_size_in )
        , maximum_size ( maximum_size_in )
        , readable     ( readable_in )
        , writable     ( writable_in )
        , display_name ( display_name_in )
    {
    }
};

//! \~japanese Extension Unit payloadを所有する値.
//! \~english Owning extension-unit payload value.
struct sCameraExtensionUnitValue
{
    sCameraExtensionUnitSelector selector;
    std::vector< std::uint8_t >  payload;

    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraExtensionUnitValue(
          const sCameraExtensionUnitSelector& selector_in = {}
        , const std::vector< std::uint8_t >& payload_in = {}
    )
        : selector ( selector_in )
        , payload  ( payload_in )
    {
    }
};

struct sCameraControlValue
{
    eCameraControl     control;
    eCameraControlMode mode;
    //! \~japanese Manual時のCapability内整数値。Automatic時は無視する。
    //! \~english Integer within the capability range for Manual mode; ignored for Automatic mode.
    std::int64_t       value;

    //! @brief Construct all members with explicit defaults.
    sCameraControlValue(
          eCameraControl control_in = eCameraControl::Exposure
        , eCameraControlMode mode_in = eCameraControlMode::Manual
        , std::int64_t value_in = 0
    )
        : control ( control_in )
        , mode    ( mode_in )
        , value   ( value_in )
    {
    }
};

struct sCameraCapability
{
    sCameraDeviceInfo                     device;
    std::vector< sCameraFormat >           formats;
    std::vector< sCameraControlCapability > controls;
    std::vector< sCameraStreamProfile >     stream_profiles;
    std::vector< sCameraExtensionUnitSelector > extension_units;

    //! @brief Construct all members with explicit defaults.
    sCameraCapability(
          const sCameraDeviceInfo& device_in = {}
        , const std::vector< sCameraFormat >& formats_in = {}
        , const std::vector< sCameraControlCapability >& controls_in = {}
        , const std::vector< sCameraStreamProfile >& stream_profiles_in = {}
        , const std::vector< sCameraExtensionUnitSelector >& extension_units_in = {}
    )
        : device          ( device_in )
        , formats         ( formats_in )
        , controls        ( controls_in )
        , stream_profiles ( stream_profiles_in )
        , extension_units ( extension_units_in )
    {
    }
};

struct sCameraFrameDescription
{
    std::uint32_t      width;
    std::uint32_t      height;
    eCameraPixelFormat pixel_format;
    std::size_t        row_stride;

    WSE_API std::size_t bytesPerPixel() const noexcept;
    WSE_API std::size_t minimumRowStride() const noexcept;
    WSE_API std::size_t memorySize() const noexcept;
    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraFrameDescription(
          std::uint32_t width_in = 0U
        , std::uint32_t height_in = 0U
        , eCameraPixelFormat pixel_format_in = eCameraPixelFormat::Unknown
        , std::size_t row_stride_in = 0U
    )
        : width        ( width_in )
        , height       ( height_in )
        , pixel_format ( pixel_format_in )
        , row_stride   ( row_stride_in )
    {
    }
};

//! \~japanese Frame Dataを所有するCopy可能な値. \~english Copyable value that owns its frame data.
struct sCameraFrame
{
    sCameraFrameDescription description;
    std::vector< std::uint8_t > data;
    std::uint64_t sequence;
    std::int64_t  monotonic_timestamp_ns;

    WSE_API bool valid() const noexcept;

    //! @brief Construct all members with explicit defaults.
    sCameraFrame(
          const sCameraFrameDescription& description_in = {}
        , const std::vector< std::uint8_t >& data_in = {}
        , std::uint64_t sequence_in = 0U
        , std::int64_t monotonic_timestamp_ns_in = 0
    )
        : description            ( description_in )
        , data                   ( data_in )
        , sequence               ( sequence_in )
        , monotonic_timestamp_ns ( monotonic_timestamp_ns_in )
    {
    }
};

struct sCameraOpenDescription
{
    sCameraDeviceInfo device;
    sCameraFormat     format;
    //! \~japanese Unknownならformatに基づくBackendの互換選択を使う.
    //! \~english Unknown uses the backend compatibility selection based on format.
    eCameraPixelFormat native_pixel_format;
    bool               allow_format_conversion;

    //! @brief Construct all members with explicit defaults.
    sCameraOpenDescription(
          const sCameraDeviceInfo& device_in = {}
        , const sCameraFormat& format_in = {}
        , eCameraPixelFormat native_pixel_format_in = eCameraPixelFormat::Unknown
        , bool allow_format_conversion_in = true
    )
        : device                  ( device_in )
        , format                  ( format_in )
        , native_pixel_format     ( native_pixel_format_in )
        , allow_format_conversion ( allow_format_conversion_in )
    {
    }
};

} // namespace tmr
} // namespace wse

#endif // WONDERSTEWENGINE_TMR_CAMERA_CAMERATYPES_H
