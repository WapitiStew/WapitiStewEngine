//*****************************************************************************************************************
//!
//! @file    wse_capi_tmr.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Tmr WebCameraの平坦C ABI.
//! @brief   \~english  Flat C ABI for the Tmr WebCamera.
//!
//! @details
//!     \~japanese
//!     @n `WebCamera`が唯一の公開Camera ownerである。Media Foundation、V4L2、libcameraのHandleは
//!        境界を越えない。
//!     @n 可変長の集合(Device一覧、Capability、Frame)は不透明Handleと添字Accessorで公開する。
//!        呼出元は対応する`*_destroy`で1回だけ解放する。
//!     @n 本ABIはNew APIのみを公開する。旧`CameraSession` Bindingや非推奨のWebCamera Methodは含めない。
//!
//!     \~english
//!     @n `WebCamera` is the sole public camera owner; no Media Foundation, V4L2, or libcamera
//!        handle crosses the boundary.
//!     @n Variable-length aggregates (device lists, capabilities, frames) are exposed as opaque
//!        handles with indexed accessors, released exactly once by the matching `*_destroy`.
//!     @n This ABI exposes the new API only; the old `CameraSession` binding and the deprecated
//!        WebCamera methods are excluded.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_CAPI_TMR_H
#define WONDERSTEWENGINE_WSE_CAPI_TMR_H

#include "wse_capi.h"

#ifdef __cplusplus
extern "C" {
#endif

//! \~japanese Camera Backend選択. `wse::tmr::eCameraBackend`と同じ値.
//! \~english  Camera backend selection with the same values as `wse::tmr::eCameraBackend`.
typedef enum wse_capi_camera_backend
{
      WSE_CAPI_CAMERA_BACKEND_AUTOMATIC         = 0
    , WSE_CAPI_CAMERA_BACKEND_MEDIA_FOUNDATION  = 1
    , WSE_CAPI_CAMERA_BACKEND_VIDEO4LINUX2      = 2
    , WSE_CAPI_CAMERA_BACKEND_LIBCAMERA         = 3
} wse_capi_camera_backend;

//! \~japanese Pixel format. `wse::tmr::eCameraPixelFormat`と同じ値.
//! \~english  Pixel format with the same values as `wse::tmr::eCameraPixelFormat`.
typedef enum wse_capi_camera_pixel_format
{
      WSE_CAPI_CAMERA_PIXEL_UNKNOWN = 0
    , WSE_CAPI_CAMERA_PIXEL_GRAY8   = 1
    , WSE_CAPI_CAMERA_PIXEL_RGB8    = 2
    , WSE_CAPI_CAMERA_PIXEL_BGR8    = 3
    , WSE_CAPI_CAMERA_PIXEL_BGRA8   = 4
    , WSE_CAPI_CAMERA_PIXEL_YUYV422 = 5
    , WSE_CAPI_CAMERA_PIXEL_NV12    = 6
    , WSE_CAPI_CAMERA_PIXEL_MJPEG   = 7
    , WSE_CAPI_CAMERA_PIXEL_GRAY16  = 8
    , WSE_CAPI_CAMERA_PIXEL_RGB16   = 9
    , WSE_CAPI_CAMERA_PIXEL_BGR16   = 10
    , WSE_CAPI_CAMERA_PIXEL_BAYER16_RGGB = 11
    , WSE_CAPI_CAMERA_PIXEL_BAYER16_BGGR = 12
    , WSE_CAPI_CAMERA_PIXEL_BAYER16_GRBG = 13
    , WSE_CAPI_CAMERA_PIXEL_BAYER16_GBRG = 14
    , WSE_CAPI_CAMERA_PIXEL_UYVY422 = 15
} wse_capi_camera_pixel_format;

//! \~japanese Transport種別. `wse::tmr::eCameraTransport`と同じ値.
//! \~english  Transport kind with the same values as `wse::tmr::eCameraTransport`.
typedef enum wse_capi_camera_transport
{
      WSE_CAPI_CAMERA_TRANSPORT_UNKNOWN = 0
    , WSE_CAPI_CAMERA_TRANSPORT_USB_UVC = 1
    , WSE_CAPI_CAMERA_TRANSPORT_CSI     = 2
    , WSE_CAPI_CAMERA_TRANSPORT_VIRTUAL = 3
    , WSE_CAPI_CAMERA_TRANSPORT_NETWORK = 4
} wse_capi_camera_transport;

//! \~japanese Control種別. `wse::tmr::eCameraControl`と同じ値.
//! \~english  Control kind with the same values as `wse::tmr::eCameraControl`.
typedef enum wse_capi_camera_control
{
      WSE_CAPI_CAMERA_CONTROL_EXPOSURE                = 0
    , WSE_CAPI_CAMERA_CONTROL_GAIN                    = 1
    , WSE_CAPI_CAMERA_CONTROL_FOCUS                   = 2
    , WSE_CAPI_CAMERA_CONTROL_BRIGHTNESS              = 3
    , WSE_CAPI_CAMERA_CONTROL_CONTRAST                = 4
    , WSE_CAPI_CAMERA_CONTROL_SATURATION              = 5
    , WSE_CAPI_CAMERA_CONTROL_WHITE_BALANCE           = 6
    , WSE_CAPI_CAMERA_CONTROL_ZOOM                    = 7
    , WSE_CAPI_CAMERA_CONTROL_IRIS                    = 8
    , WSE_CAPI_CAMERA_CONTROL_HUE                     = 9
    , WSE_CAPI_CAMERA_CONTROL_SHARPNESS               = 10
    , WSE_CAPI_CAMERA_CONTROL_GAMMA                   = 11
    , WSE_CAPI_CAMERA_CONTROL_COLOR_ENABLE            = 12
    , WSE_CAPI_CAMERA_CONTROL_BACKLIGHT_COMPENSATION  = 13
    , WSE_CAPI_CAMERA_CONTROL_PAN                     = 14
    , WSE_CAPI_CAMERA_CONTROL_TILT                    = 15
    , WSE_CAPI_CAMERA_CONTROL_ROLL                    = 16
    , WSE_CAPI_CAMERA_CONTROL_POWER_LINE_FREQUENCY    = 17
    , WSE_CAPI_CAMERA_CONTROL_FRAME_RATE              = 18
} wse_capi_camera_control;

//! \~japanese Control mode. `wse::tmr::eCameraControlMode`と同じ値.
//! \~english  Control mode with the same values as `wse::tmr::eCameraControlMode`.
typedef enum wse_capi_camera_control_mode
{
      WSE_CAPI_CAMERA_CONTROL_MODE_MANUAL    = 0
    , WSE_CAPI_CAMERA_CONTROL_MODE_AUTOMATIC = 1
} wse_capi_camera_control_mode;

//! \~japanese Control単位. `wse::tmr::eCameraControlUnit`と同じ値.
//! \~english  Control unit with the same values as `wse::tmr::eCameraControlUnit`.
typedef enum wse_capi_camera_control_unit
{
      WSE_CAPI_CAMERA_UNIT_DEVICE_NATIVE   = 0
    , WSE_CAPI_CAMERA_UNIT_MICROSECONDS    = 1
    , WSE_CAPI_CAMERA_UNIT_KELVIN          = 2
    , WSE_CAPI_CAMERA_UNIT_DIOPTERS        = 3
    , WSE_CAPI_CAMERA_UNIT_GAIN_MULTIPLIER = 4
    , WSE_CAPI_CAMERA_UNIT_RELATIVE        = 5
    , WSE_CAPI_CAMERA_UNIT_DEGREES         = 6
    , WSE_CAPI_CAMERA_UNIT_HERTZ           = 7
    , WSE_CAPI_CAMERA_UNIT_BOOLEAN         = 8
} wse_capi_camera_control_unit;

//! \~japanese Stream format. 文字列を含まないためValue structで公開する.
//! \~english  Stream format exposed as a value struct because it holds no strings.
typedef struct wse_capi_camera_format
{
    uint32_t width;
    uint32_t height;
    uint32_t frame_rate_numerator;
    uint32_t frame_rate_denominator;
    int32_t pixel_format; //!< \~japanese `wse_capi_camera_pixel_format`. \~english A pixel format.
} wse_capi_camera_format;

//! \~japanese Stream設定. `allow_conversion`が非0のとき出力Formatへの変換を許可する.
//! \~english  Stream configuration; a non-zero `allow_conversion` permits output conversion.
typedef struct wse_capi_camera_stream_configuration
{
    wse_capi_camera_format native_format;
    int32_t output_format;
    wse_capi_bool allow_conversion;
} wse_capi_camera_stream_configuration;

//! \~japanese Control値.
//! \~english  A control value.
typedef struct wse_capi_camera_control_value
{
    int32_t control;
    int32_t mode;
    int64_t value;
} wse_capi_camera_control_value;

//! \~japanese Control能力. `display_name`は別Accessorで取得する.
//! \~english  Control capability; `display_name` is read through a separate accessor.
typedef struct wse_capi_camera_control_capability
{
    int32_t control;
    int64_t minimum;
    int64_t maximum;
    int64_t step;
    int64_t default_value;
    wse_capi_bool supports_manual;
    wse_capi_bool supports_automatic;
    int32_t unit;
    double physical_scale;
    wse_capi_bool readable;
    wse_capi_bool writable;
} wse_capi_camera_control_capability;

//! \~japanese USB識別情報. `serial_number`は別Accessorで取得する.
//! \~english  USB identity; `serial_number` is read through a separate accessor.
typedef struct wse_capi_camera_usb_identity
{
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t uvc_version_bcd;
    wse_capi_bool available;
} wse_capi_camera_usb_identity;

//! \~japanese Frameの寸法とPixel配置. Byte列は別Accessorで複製する.
//! \~english  Frame dimensions and pixel layout; the bytes are copied by a separate accessor.
typedef struct wse_capi_camera_frame_description
{
    uint32_t width;
    uint32_t height;
    int32_t pixel_format;
    size_t row_stride;
} wse_capi_camera_frame_description;

//! \~japanese 列挙結果のDevice一覧Handle.
//! \~english  Handle to an enumerated device list.
typedef struct wse_capi_camera_device_list_t* wse_capi_camera_device_list;

//! \~japanese 1台分のDevice情報Handle.
//! \~english  Handle to one device description.
typedef struct wse_capi_camera_device_t* wse_capi_camera_device;

//! \~japanese Device能力Handle.
//! \~english  Handle to a device capability set.
typedef struct wse_capi_camera_capability_t* wse_capi_camera_capability;

//! \~japanese 取得済みFrame Handle.
//! \~english  Handle to one captured frame.
typedef struct wse_capi_camera_frame_t* wse_capi_camera_frame;

//! \~japanese Extension Unit選択子Handle.
//! \~english  Handle to one extension-unit selector.
typedef struct wse_capi_camera_xu_selector_t* wse_capi_camera_xu_selector;

//! \~japanese Camera Owner Handle.
//! \~english  Camera owner handle.
typedef struct wse_capi_camera_t* wse_capi_camera;

// ----------------------------------------------------------------------------------------------
// Enumeration and device description
// ----------------------------------------------------------------------------------------------

//! \~japanese 利用可能なCameraを列挙する.
//! \~english  Enumerates the available cameras.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_enumerate(
      wse_capi_camera_device_list* p_list_out
    , int32_t                      backend_in );

//! \~japanese Device一覧を解放する. `NULL`は無視する.
//! \~english  Releases a device list; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_camera_device_list_destroy(
    wse_capi_camera_device_list list_inout );

//! \~japanese 一覧の要素数を取得する.
//! \~english  Reads the number of entries in a list.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_list_count(
      wse_capi_camera_device_list list_in
    , size_t*                     p_count_out );

//! \~japanese 指定Indexのthe Deviceを複製したHandleを返す. 呼出元が解放する.
//! \~english  Returns an owned handle to the device at an index; the caller releases it.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_list_at(
      wse_capi_camera_device_list list_in
    , wse_capi_camera_device*     p_device_out
    , size_t                      index_in );

//! \~japanese Device Handleを解放する. `NULL`は無視する.
//! \~english  Releases a device handle; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_camera_device_destroy( wse_capi_camera_device device_inout );

//! \~japanese Deviceが所属するBackendを取得する.
//! \~english  Reads the backend a device belongs to.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_backend(
      wse_capi_camera_device device_in
    , int32_t*               p_backend_out );

//! \~japanese Device種別のTransportを取得する.
//! \~english  Reads the transport kind of a device.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_transport_type(
      wse_capi_camera_device device_in
    , int32_t*               p_transport_out );

//! \~japanese Deviceの安定IDを取得する. 2回呼出方式である.
//! \~english  Reads the stable device id using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_id(
      wse_capi_camera_device device_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 capacity_in );

//! \~japanese Deviceの表示名を取得する. 2回呼出方式である.
//! \~english  Reads the device display name using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_display_name(
      wse_capi_camera_device device_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 capacity_in );

//! \~japanese DeviceのTransport説明を取得する. 2回呼出方式である.
//! \~english  Reads the device transport description using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_transport(
      wse_capi_camera_device device_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 capacity_in );

//! \~japanese DeviceのUSB識別情報を取得する.
//! \~english  Reads the USB identity of a device.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_usb(
      wse_capi_camera_device        device_in
    , wse_capi_camera_usb_identity* p_usb_out );

//! \~japanese DeviceのUSB Serial番号を取得する. 2回呼出方式である.
//! \~english  Reads the USB serial number using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_device_usb_serial_number(
      wse_capi_camera_device device_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 capacity_in );

// ----------------------------------------------------------------------------------------------
// Capability
// ----------------------------------------------------------------------------------------------

//! \~japanese 指定Deviceの能力を取得する.
//! \~english  Reads the capability set of a device.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capabilities(
      wse_capi_camera_device      device_in
    , wse_capi_camera_capability* p_capability_out );

//! \~japanese 能力Handleを解放する. `NULL`は無視する.
//! \~english  Releases a capability handle; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_camera_capability_destroy(
    wse_capi_camera_capability capability_inout );

//! \~japanese 対応Formatの個数を取得する.
//! \~english  Reads the number of supported formats.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_format_count(
      wse_capi_camera_capability capability_in
    , size_t*                    p_count_out );

//! \~japanese 指定IndexのFormatを取得する.
//! \~english  Reads the format at an index.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_format_at(
      wse_capi_camera_capability capability_in
    , wse_capi_camera_format*    p_format_out
    , size_t                     index_in );

//! \~japanese Stream Profileの個数を取得する.
//! \~english  Reads the number of stream profiles.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_profile_count(
      wse_capi_camera_capability capability_in
    , size_t*                    p_count_out );

//! \~japanese 指定IndexのStream ProfileのNative Formatを取得する.
//! \~english  Reads the native format of the stream profile at an index.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_profile_native_format(
      wse_capi_camera_capability capability_in
    , wse_capi_camera_format*    p_format_out
    , size_t                     index_in );

//! \~japanese 指定IndexのStream Profileが提供する出力Format数を取得する.
//! \~english  Reads how many output formats the stream profile at an index offers.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_profile_output_count(
      wse_capi_camera_capability capability_in
    , size_t*                    p_count_out
    , size_t                     index_in );

//! \~japanese 指定Stream Profileの出力Formatを取得する.
//! \~english  Reads one output format of a stream profile.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_profile_output_at(
      wse_capi_camera_capability capability_in
    , int32_t*                   p_pixel_format_out
    , size_t                     profile_index_in
    , size_t                     output_index_in );

//! \~japanese Control能力の個数を取得する.
//! \~english  Reads the number of control capabilities.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_control_count(
      wse_capi_camera_capability capability_in
    , size_t*                    p_count_out );

//! \~japanese 指定IndexのControl能力を取得する.
//! \~english  Reads the control capability at an index.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_control_at(
      wse_capi_camera_capability          capability_in
    , wse_capi_camera_control_capability* p_control_out
    , size_t                              index_in );

//! \~japanese 指定IndexのControl表示名を取得する. 2回呼出方式である.
//! \~english  Reads the control display name at an index using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_control_display_name(
      wse_capi_camera_capability capability_in
    , char*                      p_buffer_out
    , size_t*                    p_size_out
    , size_t                     index_in
    , size_t                     capacity_in );

//! \~japanese Extension Unitの個数を取得する.
//! \~english  Reads the number of extension units.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_extension_unit_count(
      wse_capi_camera_capability capability_in
    , size_t*                    p_count_out );

//! \~japanese 指定IndexのExtension Unit選択子を複製して返す. 呼出元が解放する.
//! \~english  Returns an owned extension-unit selector at an index; the caller releases it.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_extension_unit_at(
      wse_capi_camera_capability   capability_in
    , wse_capi_camera_xu_selector* p_selector_out
    , size_t                       index_in );

//! \~japanese 能力が指すDeviceを複製して返す. 呼出元が解放する.
//! \~english  Returns an owned copy of the device the capability describes.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_capability_device(
      wse_capi_camera_capability capability_in
    , wse_capi_camera_device*    p_device_out );

// ----------------------------------------------------------------------------------------------
// Extension unit
// ----------------------------------------------------------------------------------------------

//! \~japanese Extension Unit選択子を解放する. `NULL`は無視する.
//! \~english  Releases an extension-unit selector; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_camera_xu_selector_destroy(
    wse_capi_camera_xu_selector selector_inout );

//! \~japanese 選択子のUnit ID、Selector、Size範囲および可否を取得する.
//! \~english  Reads the unit id, selector, size range, and access flags.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_xu_selector_describe(
      wse_capi_camera_xu_selector selector_in
    , uint8_t*                    p_unit_id_out
    , uint8_t*                    p_selector_out
    , size_t*                     p_minimum_size_out
    , size_t*                     p_maximum_size_out
    , wse_capi_bool*              p_readable_out
    , wse_capi_bool*              p_writable_out );

//! \~japanese 選択子の16 byteの`unit_guid`を複製する.
//! \~english  Copies the 16-byte selector `unit_guid`.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_xu_selector_guid(
      wse_capi_camera_xu_selector selector_in
    , uint8_t*                    p_buffer_out
    , size_t                      capacity_in );

//! \~japanese 選択子の表示名を取得する. 2回呼出方式である.
//! \~english  Reads the selector display name using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_xu_selector_display_name(
      wse_capi_camera_xu_selector selector_in
    , char*                       p_buffer_out
    , size_t*                     p_size_out
    , size_t                      capacity_in );

// ----------------------------------------------------------------------------------------------
// Frame
// ----------------------------------------------------------------------------------------------

//! \~japanese Frameを解放する. `NULL`は無視する.
//! \~english  Releases a frame; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_camera_frame_destroy( wse_capi_camera_frame frame_inout );

//! \~japanese Frameの向き補正操作. \~english Orientation applied to a frame.
typedef enum wse_capi_image_orientation
{
      WSE_CAPI_IMAGE_ORIENTATION_NONE            = 0
    , WSE_CAPI_IMAGE_ORIENTATION_ROTATE_90_CW    = 1
    , WSE_CAPI_IMAGE_ORIENTATION_ROTATE_180      = 2
    , WSE_CAPI_IMAGE_ORIENTATION_ROTATE_90_CCW   = 3
    , WSE_CAPI_IMAGE_ORIENTATION_FLIP_HORIZONTAL = 4
    , WSE_CAPI_IMAGE_ORIENTATION_FLIP_VERTICAL   = 5
} wse_capi_image_orientation;

//! \~japanese 左上2x2のBayer配列. \~english The Bayer layout of the top-left two-by-two block.
typedef enum wse_capi_bayer_pattern
{
      WSE_CAPI_BAYER_PATTERN_RGGB = 0
    , WSE_CAPI_BAYER_PATTERN_BGGR = 1
    , WSE_CAPI_BAYER_PATTERN_GRBG = 2
    , WSE_CAPI_BAYER_PATTERN_GBRG = 3
} wse_capi_bayer_pattern;

//!
//! @brief
//!     \~japanese BayerをColorへ起こす方式.
//!     \~english  How a Bayer frame is raised to colour.
//!
//! @details
//!     \~japanese
//!         `BLOCK_2X2`は2x2の4 Sampleをそのまま4画素へ複製するため、Sample値が変わらない。
//!      @n `BILINEAR`は近傍を平均するため、端が滑らかになる。用途で選ぶ。
//!     \~english
//!         `BLOCK_2X2` copies a block's four samples onto its four pixels, so no sample value
//!         changes; `BILINEAR` averages each pixel's neighbours, so edges come out smoother.
//!
typedef enum wse_capi_demosaic_method
{
      WSE_CAPI_DEMOSAIC_METHOD_BLOCK_2X2 = 0
    , WSE_CAPI_DEMOSAIC_METHOD_BILINEAR  = 1
} wse_capi_demosaic_method;

//! \~japanese Pixel Formatが向き補正に対応するか. \~english Whether a pixel format can be reoriented.
WSE_CAPI wse_capi_bool WSE_CAPI_CALL wse_capi_camera_pixel_format_supports_orientation(
    int32_t pixel_format_in );

//! \~japanese Pixel Formatが平均合成に対応するか. \~english Whether a pixel format can be averaged.
WSE_CAPI wse_capi_bool WSE_CAPI_CALL wse_capi_camera_pixel_format_supports_averaging(
    int32_t pixel_format_in );

//! \~japanese Pixel FormatがBayer配列か. \~english Whether a pixel format carries a Bayer layout.
WSE_CAPI wse_capi_bool WSE_CAPI_CALL wse_capi_camera_pixel_format_is_bayer(
    int32_t pixel_format_in );

//! \~japanese Bayer FormatのBayer配列を取得する. Bayerでなければ失敗する.
//! \~english  Reads a Bayer format's layout; fails for a format that is not Bayer.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_pixel_format_bayer_pattern(
      int32_t* p_pattern_out
    , int32_t  pixel_format_in );

//!
//! @brief
//!     \~japanese 向きを補正した新しいFrameを作る.
//!     \~english  Produces a new frame whose orientation has been corrected.
//!
//! @details
//!     \~japanese
//!         `ROTATE_90_CW`と`ROTATE_90_CCW`では幅と高さが入れ替わる。
//!      @n Bayer Frameは画素を移動すると別の色のSiteへ移るため拒否する。先にDemosaicする。
//!     \~english
//!         `ROTATE_90_CW` and `ROTATE_90_CCW` exchange the width and the height.
//!      @n A Bayer frame is refused, because moving its pixels puts them on sites of other colours;
//!         convert it first.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_apply_orientation(
      wse_capi_camera_frame  frame_in
    , wse_capi_camera_frame* p_frame_out
    , int32_t                orientation_in );

//!
//! @brief
//!     \~japanese Bayer FrameをColor Frameへ変換した新しいFrameを作る.
//!     \~english  Produces a new colour frame from a Bayer one.
//!
//! @details
//!     \~japanese
//!         出力Formatは`RGB8`、`BGR8`、`RGB16`および`BGR16`から選ぶ。8bit出力は上位8bitを採る。
//!     \~english
//!         The result format is one of `RGB8`, `BGR8`, `RGB16`, and `BGR16`; an eight-bit result
//!         keeps the high eight bits.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_demosaic(
      wse_capi_camera_frame  frame_in
    , wse_capi_camera_frame* p_frame_out
    , int32_t                output_pixel_format_in
    , int32_t                method_in );

//! \~japanese 同一形状のFrameを加算し平均を取り出す累積器.
//! \~english  Accumulates frames of one shape and produces their average.
typedef struct wse_capi_camera_frame_accumulator_t* wse_capi_camera_frame_accumulator;

WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_accumulator_create(
    wse_capi_camera_frame_accumulator* p_accumulator_out );

//! \~japanese 累積器を解放する. `NULL`は無視する.
//! \~english  Releases an accumulator; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_camera_frame_accumulator_destroy(
    wse_capi_camera_frame_accumulator accumulator_inout );

//! \~japanese Frameを1枚加算する. 記述子が異なると失敗し、累積は変わらない.
//! \~english  Adds one frame; a differing description fails and leaves the accumulation alone.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_accumulator_add(
      wse_capi_camera_frame_accumulator accumulator_inout
    , wse_capi_camera_frame             frame_in );

WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_accumulator_count(
      wse_capi_camera_frame_accumulator accumulator_in
    , size_t*                           p_count_out );

//! \~japanese 平均Frameを作る. 丸めは四捨五入. \~english Produces the averaged frame, rounded to nearest.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_accumulator_average(
      wse_capi_camera_frame_accumulator accumulator_in
    , wse_capi_camera_frame*            p_frame_out );

//! \~japanese 累積を破棄する. \~english Discards the accumulation.
WSE_CAPI void WSE_CAPI_CALL wse_capi_camera_frame_accumulator_reset(
    wse_capi_camera_frame_accumulator accumulator_inout );

//! \~japanese Frameの寸法、Pixel配置、Sequence およびTimestampを取得する.
//! \~english  Reads the frame dimensions, pixel layout, sequence, and timestamp.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_describe(
      wse_capi_camera_frame              frame_in
    , wse_capi_camera_frame_description* p_description_out
    , uint64_t*                          p_sequence_out
    , int64_t*                           p_monotonic_timestamp_ns_out );

//! \~japanese Frame Byte列を複製する. 2回呼出方式である.
//! \~english  Copies the frame bytes using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_frame_data(
      wse_capi_camera_frame frame_in
    , uint8_t*              p_buffer_out
    , size_t*               p_size_out
    , size_t                capacity_in );

// ----------------------------------------------------------------------------------------------
// Camera owner
// ----------------------------------------------------------------------------------------------

//! \~japanese Cameraを生成する.
//! \~english  Creates a camera.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_create( wse_capi_camera* p_camera_out );

//! \~japanese Cameraを解放する. Streamingを停止し接続を閉じる. `NULL`は無視する.
//! \~english  Releases a camera, stopping streaming and closing it; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_camera_destroy( wse_capi_camera camera_inout );

//!
//! \~japanese
//! @brief   Deviceを開く.
//! @param [in,out] camera_inout  開く先のCamera handle. `NULL`のときInvalidArgumentを返す.
//! @param [in] device_in         開くDevice. `NULL`のときInvalidArgumentを返す.
//! @param [in] configuration_in  Stream設定. `NULL`のときBackendの既定Streamを使う.
//! \~english
//! @brief   Opens a device.
//! @param [in,out] camera_inout  Camera the device is opened into; `NULL` yields InvalidArgument.
//! @param [in] device_in         Device to open; `NULL` yields InvalidArgument.
//! @param [in] configuration_in  Stream configuration, or `NULL` to use the backend default stream.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_open(
      wse_capi_camera                             camera_inout
    , wse_capi_camera_device                      device_in
    , const wse_capi_camera_stream_configuration* configuration_in );

//! \~japanese Streamingを開始する. Frameは`wse_capi_camera_read_frame`で引き取る.
//! \~english  Starts streaming. Frames are collected with `wse_capi_camera_read_frame`.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_start( wse_capi_camera camera_inout );

//!
//! \~japanese
//!     Callbackへ渡す呼出元Token. WSEは中身を読まず、そのまま返すだけである.
//!  @n Pointerを渡す場合は呼出元がその生存期間を保証する.
//! \~english
//!     Caller token handed straight back to the callback. WSE never reads it.
//!  @n A caller that passes a pointer through it owns that pointer's lifetime.
//!
typedef intptr_t wse_capi_user_data;

//!
//! \~japanese
//!     Frame 1枚ごとに呼ばれるCallback.
//!  @n `frame_in`の所有権はCallbackへ渡る. 他のHandleと同じく`wse_capi_camera_frame_destroy`で
//!     解放する責任がある. 借用ではないため、Callbackを抜けた後も保持してよい.
//!  @n 撮影に失敗した回は`status_in`が失敗を報告し、`frame_in`は`NULL`である.
//!  @n Frame handleの確保／Copy失敗もNULLと失敗Statusで1回通知した後、配信を停止する.
//!     Native streamingの停止／JoinはOwner側から行う. 言語境界へ例外を渡してはならない.
//! \~english
//!     Called once per frame.
//!  @n Ownership of `frame_in` passes to the callback, which releases it with
//!     `wse_capi_camera_frame_destroy` like every other handle. It is not borrowed, so the
//!     callback may keep it after returning.
//!  @n On a capture that failed, `status_in` reports the failure and `frame_in` is `NULL`.
//!  @n Frame-handle allocation/copy failure also reports NULL and a failure status once, then
//!     stops delivery. The owner must stop/join native streaming. Do not throw across a language boundary.
//!
typedef void ( WSE_CAPI_CALL *wse_capi_camera_frame_callback )(
      wse_capi_camera_frame frame_in
    , wse_capi_status       status_in
    , wse_capi_user_data    user_data_in );

//!
//! \~japanese
//! @brief   Callback付きでStreamingを開始する.
//! @details Callbackは撮影Threadから呼ばれるため、呼出元は自身のThread安全性に責任を持つ.
//! @param [in,out] camera_inout  Streamingを開始するCamera handle. `NULL`のときInvalidArgumentを返す.
//! @param [in] callback_in   Frameごとに呼ぶ関数. `NULL`不可.
//! @param [in] user_data_in  呼出元へそのまま返すToken.
//! \~english
//! @brief   Starts streaming with a callback.
//! @details The callback runs on the capture thread, so the caller owns its own thread safety.
//! @param [in,out] camera_inout  Camera to start streaming on; `NULL` yields InvalidArgument.
//! @param [in] callback_in   Function called once per frame; must not be `NULL`.
//! @param [in] user_data_in  Token handed straight back to the caller.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_start_with_callback(
      wse_capi_camera                camera_inout
    , wse_capi_camera_frame_callback callback_in
    , wse_capi_user_data             user_data_in );

//! \~japanese Streamingを停止する.
//! \~english  Stops streaming.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_stop( wse_capi_camera camera_inout );

//! \~japanese Frameを1枚、明示Timeoutで取得する.
//! \~english  Reads one frame with an explicit timeout.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_read_frame(
      wse_capi_camera        camera_inout
    , wse_capi_camera_frame* p_frame_out
    , uint32_t               timeout_milliseconds_in );

//!
//! @brief
//!     \~japanese 指定枚数を読み出し、その平均Frameを作る.
//!     \~english  Reads a requested number of frames and produces their average.
//!
//! @details
//!     \~japanese
//!         読み出しの前にStreamingしている必要がある。1回でも失敗すればそこで中断する。
//!     \~english
//!         The camera has to be streaming first, and the first failed read ends the loop.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_read_averaged_frame(
      wse_capi_camera        camera_inout
    , wse_capi_camera_frame* p_frame_out
    , size_t                 count_in
    , uint32_t               timeout_milliseconds_in );

//! \~japanese 現在開いているDeviceの能力を取得する.
//! \~english  Reads the capability set of the currently open device.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_current_capabilities(
      wse_capi_camera             camera_in
    , wse_capi_camera_capability* p_capability_out );

//! \~japanese 指定Controlの能力を取得する.
//! \~english  Reads the capability of one control.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_query_control_capability(
      wse_capi_camera                     camera_in
    , wse_capi_camera_control_capability* p_capability_out
    , int32_t                             control_in );

//! \~japanese 指定Controlの現在値を取得する.
//! \~english  Reads the current value of one control.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_get_control(
      wse_capi_camera                camera_inout
    , wse_capi_camera_control_value* p_value_out
    , int32_t                        control_in );

//! \~japanese 指定Controlへ値を設定する.
//! \~english  Writes one control value.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_set_control(
      wse_capi_camera                      camera_inout
    , const wse_capi_camera_control_value* value_in );

//! \~japanese Extension Unitの値を読み出す. 2回呼出方式である.
//! \~english  Reads an extension-unit value using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_get_extension_unit(
      wse_capi_camera             camera_inout
    , uint8_t*                    p_buffer_out
    , size_t*                     p_size_out
    , wse_capi_camera_xu_selector selector_in
    , size_t                      capacity_in );

//! \~japanese Extension Unitへ値を書き込む.
//! \~english  Writes an extension-unit value.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_set_extension_unit(
      wse_capi_camera             camera_inout
    , wse_capi_camera_xu_selector selector_in
    , const uint8_t*              payload_in
    , size_t                      size_in );

//! \~japanese Deviceが開いている場合に非0を返す.
//! \~english  Reports non-zero while a device is open.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_is_open(
      wse_capi_camera camera_in
    , wse_capi_bool*  p_open_out );

//! \~japanese Streaming中の場合に非0を返す.
//! \~english  Reports non-zero while streaming.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_is_streaming(
      wse_capi_camera camera_in
    , wse_capi_bool*  p_streaming_out );

//! \~japanese Cameraを閉じる. 終端かつ冪等である.
//! \~english  Closes the camera; the operation is terminal and idempotent.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_camera_close( wse_capi_camera camera_inout );

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WONDERSTEWENGINE_WSE_CAPI_TMR_H
