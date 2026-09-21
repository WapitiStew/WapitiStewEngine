//*****************************************************************************************************************
//!
//! @file    CameraFrameOps.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Sep-02, 2026   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese Camera Frameに対する向き補正と平均合成を定義するファイル。
//!     \~english  Defines orientation correction and average composition for a camera frame.
//!
//!
//! @details
//!     \~japanese
//!         画像演算そのものはCoreの`wse_ImageTransform.h`が持ち、本ファイルはCamera側の責務だけを担う。
//!      @n すなわちCamera Pixel FormatとCore Pixel Formatの対応、非対応形式の明示拒否、
//!      @n およびDeviceから所定枚数を読み出す手続きである。
//!      @n `WebCamera`にMethodは追加しない。Device制御と画像処理を混在させないためである。
//!     \~english
//!         The image operations themselves live in Core `wse_ImageTransform.h`; this file carries
//!         only the camera-side responsibility, which is mapping a camera pixel format onto a Core
//!         pixel format, refusing a format that cannot be mapped, and reading a requested number of
//!         frames from a device.
//!      @n No method is added to `WebCamera`, so device control and image processing stay apart.
//!
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#pragma once

#ifndef WONDERSTEWENGINE_TMR_CAMERA_CAMERAFRAMEOPS_H
#define WONDERSTEWENGINE_TMR_CAMERA_CAMERAFRAMEOPS_H

#include "../../dynamic.h"
#include "../../wse/data/wse_Image.h"
#include "../../wse/data/wse_ImageDemosaic.h"
#include "../../wse/data/wse_ImageInterleaved.h"
#include "../../wse/data/wse_ImageTransform.h"
#include "CameraError.h"
#include "CameraTypes.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace wse
{
namespace tmr
{

class WebCamera;

//!
//! @brief
//!     \~japanese Camera Frameが向き補正に対応しているかを返す。
//!     \~english  Returns whether a camera frame can be reoriented by this file.
//!
//! @details
//!     \~japanese
//!         対応するのは`Gray8`、`Rgb8`、`Bgr8`、`Bgra8`、`Gray16`、`Rgb16`および`Bgr16`である。
//!      @n `Yuyv422`、`Uyvy422`、`Nv12`はSubsamplingを、`Mjpeg`はCompressionを伴うため、
//!      @n 画素を単位とする移動の意味が定義できず対応しない。
//!      @n Bayer Formatも、画素を移動すると別の色のSiteへ移るため対応しない。
//!     \~english
//!         `Gray8`, `Rgb8`, `Bgr8`, `Bgra8`, `Gray16`, `Rgb16`, and `Bgr16` are supported.
//!      @n `Yuyv422`, `Uyvy422`, and `Nv12` are subsampled and `Mjpeg` is compressed, so moving whole pixels has
//!      @n no defined meaning for them.
//!      @n A Bayer format is not supported either, because moving a pixel puts it on a site of
//!      @n another colour.
//!
//! @param[in] format_in \~japanese 判定するFormat. \~english Format to test.
//!
WSE_API bool isFrameOperationSupported( eCameraPixelFormat format_in ) noexcept;

//!
//! @brief
//!     \~japanese Camera Frameが平均合成に対応しているかを返す。
//!     \~english  Returns whether a camera frame can be averaged by this file.
//!
//! @details
//!     \~japanese
//!         `isFrameOperationSupported()`が真となるFormatに加え、Bayer Formatにも対応する。
//!      @n 平均合成はFrame間で同じ位置のSample同士を足すだけであり、Siteの色は変わらないためである。
//!     \~english
//!         Every format that `isFrameOperationSupported()` accepts, and a Bayer format as well.
//!      @n Averaging only adds the sample at one position to the sample at the same position in
//!      @n another frame, so a site keeps its colour.
//!
WSE_API bool isFrameAveragingSupported( eCameraPixelFormat format_in ) noexcept;

//!
//! @brief
//!     \~japanese FormatがBayer配列かどうかを返す.
//!     \~english  Returns whether a format carries a Bayer layout.
//!
WSE_API bool isBayerFormat( eCameraPixelFormat format_in ) noexcept;

//!
//! @brief
//!     \~japanese Bayer FormatのBayer配列を返す.
//!     \~english  Returns the Bayer layout a Bayer format carries.
//!
//! @retval \~japanese Bayer Formatでなければ`false`を返し、`p_pattern_out`は変更しない.
//!         \~english  Returns false for a non-Bayer format and leaves `p_pattern_out` unchanged.
//!
WSE_API bool bayerPatternOf(
      wse::eBayerPattern* p_pattern_out
    , eCameraPixelFormat  format_in ) noexcept;

//!
//! @brief
//!     \~japanese Bayer FrameをColor Frameへ変換する.
//!     \~english  Converts a Bayer frame into a colour frame.
//!
//! @details
//!     \~japanese
//!         出力Formatは`Rgb8`、`Bgr8`、`Rgb16`および`Bgr16`から選ぶ。8bit出力では上位8bitを採用する。
//!      @n Bayer Frameは回転や反転を適用できないため、向きの補正は本変換の後に行う。
//!      @n 一方で平均合成は変換の前に行える。Noise低減はBayerのまま行う方が結果がよい。
//!     \~english
//!         The result format is one of `Rgb8`, `Bgr8`, `Rgb16`, and `Bgr16`; an eight-bit result
//!         keeps the high eight bits.
//!      @n A Bayer frame cannot be rotated or mirrored, so correct the orientation after converting.
//!      @n Averaging, on the other hand, belongs before the conversion, where reducing noise on the
//!      @n raw sites gives the better result.
//!
//! @param[in] frame_in         \~japanese 入力Bayer Frame. \~english Bayer source frame.
//! @param[in] output_format_in \~japanese 出力Format.      \~english Result format.
//! @param[in] method_in        \~japanese Demosaic方式.    \~english Demosaic method.
//!
//! @retval UnsupportedFormat \~japanese 入力がBayerでない、または出力Formatが対象外の場合.
//!                           \~english  When the source is not Bayer or the result format is not one of the four.
//!
WSE_API CameraResult< sCameraFrame > demosaicFrame(
      const sCameraFrame&  frame_in
    , eCameraPixelFormat   output_format_in
    , wse::eDemosaicMethod method_in = wse::eDemosaicMethod::Bilinear );

//!
//! @brief
//!     \~japanese Camera Frameの向きを補正した新しいFrameを返す。
//!     \~english  Returns a new frame whose orientation has been corrected.
//!
//! @details
//!     \~japanese
//!         入力のPixel Formatは維持する。`Rotate90CW`と`Rotate90CCW`では幅と高さが入れ替わり、
//!      @n `row_stride`は補正後の幅から再計算する。`sequence`と`monotonic_timestamp_ns`は引き継ぐ。
//!     \~english
//!         The pixel format is preserved. `Rotate90CW` and `Rotate90CCW` exchange the width and the
//!         height, and `row_stride` is recomputed from the corrected width. The sequence number and
//!         the monotonic timestamp are carried over.
//!
//! @param[in] frame_in       \~japanese 入力Frame.     \~english Source frame.
//! @param[in] orientation_in \~japanese 適用する操作.  \~english Operation to apply.
//!
//! @retval 成功 \~japanese 補正後のFrame. \~english The corrected frame.
//! @retval UnsupportedFormat \~japanese 非対応Pixel Formatの場合. \~english For an unsupported pixel format.
//! @retval InvalidArgument   \~japanese Frameが無効な場合.        \~english When the frame is invalid.
//!
WSE_API CameraResult< sCameraFrame > applyOrientation(
      const sCameraFrame&     frame_in
    , wse::eImageOrientation  orientation_in );

//!
//! @class CameraFrameAccumulator
//!
//! @brief
//!     \~japanese 同一形状のCamera Frameを加算し、平均を取り出す累積器。
//!     \~english  Accumulates camera frames of one shape and produces their average.
//!
//! @details
//!     \~japanese
//!         保持するのは累積Bufferだけであり、加算した枚数に依存しない。
//!      @n 同期読み出しとCallback受信のどちらからでも加算できる。
//!      @n 記述子が異なるFrameを加算しようとした場合は失敗を返し、累積内容は変更しない。
//!      @n 加算はSample単位で行うため、16bit Formatでも上位byteと下位byteは分かれない。
//!     \~english
//!         Only an accumulation buffer is held, independently of how many frames were added.
//!      @n Frames may be added from a synchronous read or from a streaming callback.
//!      @n Adding a frame whose description differs fails and leaves the accumulation unchanged.
//!      @n Accumulation works one whole sample at a time, so a sixteen-bit format keeps its two
//!      @n bytes together.
//!
class WSE_API CameraFrameAccumulator final
{
  private:
    class Impl;
    std::unique_ptr< Impl > m_impl;

  public:
    CameraFrameAccumulator();
    ~CameraFrameAccumulator();
    CameraFrameAccumulator( const CameraFrameAccumulator& ) = delete;
    CameraFrameAccumulator& operator=( const CameraFrameAccumulator& ) = delete;
    CameraFrameAccumulator( CameraFrameAccumulator&& other_inout ) noexcept;
    CameraFrameAccumulator& operator=( CameraFrameAccumulator&& other_inout ) noexcept;

    //! \~japanese Frameを1枚加算する. \~english Adds one frame.
    CameraStatus add( const sCameraFrame& frame_in );

    //! \~japanese 加算した枚数. \~english Number of accumulated frames.
    std::size_t count() const noexcept;

    //! \~japanese 累積中の記述子. 未加算ならすべて既定値. \~english Description being accumulated.
    sCameraFrameDescription description() const noexcept;

    //! \~japanese 平均Frameを返す. 丸めは四捨五入. \~english Returns the averaged frame, rounded to nearest.
    CameraResult< sCameraFrame > average() const;

    //! \~japanese 累積を破棄する. \~english Discards the accumulation.
    void reset() noexcept;
};

//!
//! @brief
//!     \~japanese Cameraから指定枚数を読み出し、その平均Frameを返す。
//!     \~english  Reads a requested number of frames from a camera and returns their average.
//!
//! @details
//!     \~japanese
//!         `readFrame()`を`count_in`回呼び、`CameraFrameAccumulator`へ加算するだけである。
//!      @n Cameraは呼び出し前にStreamingしている必要がある。1回でも読み出しが失敗した時点で中断し、
//!      @n その失敗をそのまま返す。
//!     \~english
//!         Calls `readFrame()` `count_in` times and adds each result to a `CameraFrameAccumulator`.
//!      @n The camera has to be streaming before the call. The first failed read aborts the loop and
//!      @n is returned unchanged.
//!
//! @param[in,out] p_camera_inout \~japanese 読み出し元Camera. \~english Camera to read from.
//! @param[in] count_in     \~japanese 読み出す枚数.     \~english Number of frames to read.
//! @param[in] timeout_ms_in \~japanese 1枚あたりの待機上限. \~english Per-frame timeout.
//!
//! @retval 成功 \~japanese 平均Frame. \~english The averaged frame.
//! @retval InvalidArgument \~japanese `count_in`が0の場合. \~english When `count_in` is zero.
//!
WSE_API CameraResult< sCameraFrame > readAveragedFrame(
      WebCamera*    p_camera_inout
    , std::size_t   count_in
    , std::uint32_t timeout_ms_in );


//!
//! @brief
//!     \~japanese Camera FormatをCoreのPixel Formatへ写す。
//!     \~english  Maps a camera format onto a Core pixel format.
//!
//! @details
//!     \~japanese
//!         色変換を伴わずに写せるFormatだけを受け付ける。`Bgr8`のようにChannel 0がBのFormatは、
//!      @n 順序を保ったまま`BGR3D8`などへ写る。`Bgra8`は4 Channelのまま`BGRA4D8`へ写り、
//!      @n Alphaを落とさない。Bayer FormatはSampleの入れ物として`CH1D16`へ写るが、Bayer配列は
//!      @n Formatに乗らないため`bayerPatternOf()`で別に受け取る。
//!      @n `Yuyv422`、`Uyvy422`、`Nv12`、`Mjpeg`および`Unknown`は`false`を返す。
//!     \~english
//!         Accepts only a format that maps across without a colour conversion. A format whose
//!         channel zero is blue, such as `Bgr8`, keeps its order and maps onto `BGR3D8`. `Bgra8`
//!         maps onto `BGRA4D8` and stays four channels, so its alpha survives. A Bayer format maps
//!         onto `CH1D16` as a carrier for its samples, but the Bayer layout does not travel in the
//!         format, so take it from `bayerPatternOf()` separately.
//!      @n `Yuyv422`, `Uyvy422`, `Nv12`, `Mjpeg`, and `Unknown` report false.
//!
//! @param[in]  format_in  \~japanese 写すCamera Format. \~english Camera format to map.
//! @param[out] p_format_out \~japanese 対応するCore Format. \~english The Core format it maps onto.
//!
//! @retval false \~japanese 色変換なしでは写せないFormat. `format_out`は変更しない.
//!         \~english  A format that cannot map across without a conversion; `p_format_out` is untouched.
//!
WSE_API bool coreFormatOf(
      wse::ePixFormat*   p_format_out
    , eCameraPixelFormat format_in ) noexcept;

//!
//! @brief
//!     \~japanese Camera Frameを`wse::Image_`へ変換できるかを返す。
//!     \~english  Returns whether a camera frame can be converted into a `wse::Image_`.
//!
//! @details
//!     \~japanese
//!         `coreFormatOf()`が受け付けるFormatに加え、`Yuyv422`、`Uyvy422`、`Nv12`を含む。これらはBT.601で
//!      @n RGBへ変換するため、`wse::img3c08_t`への変換だけが可能である。
//!      @n `Mjpeg`は圧縮されており、Engineは復号器を持たないため対応しない。
//!     \~english
//!         Every format `coreFormatOf()` accepts, plus `Yuyv422`, `Uyvy422`, and `Nv12`. These formats convert to
//!         RGB with the BT.601 matrix, so only a conversion into `wse::img3c08_t` is available.
//!      @n `Mjpeg` is compressed and the engine carries no decoder, so it is not supported.
//!
WSE_API bool isFrameImageConversionSupported( eCameraPixelFormat format_in ) noexcept;

//!
//! @brief
//!     \~japanese Camera FrameをCoreの`wse::Image_`へ変換する。
//!     \~english  Converts a camera frame into a Core `wse::Image_`.
//!
//! @details
//!     \~japanese
//!         宛先の型がFormatを選ぶ。Frameが宛先の型へ写らない場合は`UnsupportedFormat`を返し、
//!      @n `image_out`は変更しない。行Strideは畳み込まれ、結果は常に詰まった配置になる。
//!      @n `wse::img3c08_t`への変換だけは`Yuyv422`、`Uyvy422`、`Nv12`も受け付け、BT.601でRGBへ変換する。
//!     \~english
//!         The destination type selects the format. A frame that does not map onto that type gives
//!         `UnsupportedFormat` and leaves `image_out` untouched. The row stride is folded away, so
//!         the result is always packed.
//!      @n The conversion into `wse::img3c08_t` additionally accepts `Yuyv422`, `Uyvy422`, and `Nv12` and
//!      @n converts them to RGB with the BT.601 matrix.
//!
//! @param[out] p_image_out \~japanese 変換結果.    \~english The converted image.
//! @param[in]  frame_in    \~japanese 変換元Frame. \~english Frame to convert.
//!
//! \~
//! @{
WSE_API CameraStatus toImage( wse::img1c08_t*      p_image_out, const sCameraFrame& frame_in );
WSE_API CameraStatus toImage( wse::img1c16_t*      p_image_out, const sCameraFrame& frame_in );
WSE_API CameraStatus toImage( wse::img3c08_t*      p_image_out, const sCameraFrame& frame_in );
WSE_API CameraStatus toImage( wse::img3c08_bgr_t*  p_image_out, const sCameraFrame& frame_in );
WSE_API CameraStatus toImage( wse::img3c16_t*      p_image_out, const sCameraFrame& frame_in );
WSE_API CameraStatus toImage( wse::img3c16_bgr_t*  p_image_out, const sCameraFrame& frame_in );
WSE_API CameraStatus toImage( wse::img4c08_bgra_t* p_image_out, const sCameraFrame& frame_in );
//! @}

//!
//! @brief
//!     \~japanese Coreの`wse::Image_`をCamera Frameへ変換する。
//!     \~english  Converts a Core `wse::Image_` into a camera frame.
//!
//! @details
//!     \~japanese
//!         元の型がCamera Formatを選ぶ。結果は行Strideに余白の無い詰まったFrameであり、
//!      @n `sequence`と`monotonic_timestamp_ns`は0になる。
//!     \~english
//!         The source type selects the camera format. The result is a packed frame with no row
//!         margin, and its `sequence` and `monotonic_timestamp_ns` are zero.
//!
//! @param[in] image_in \~japanese 変換元Image. \~english Image to convert.
//!
//! \~
//! @{
WSE_API CameraResult< sCameraFrame > toCameraFrame( const wse::img1c08_t&      image_in );
WSE_API CameraResult< sCameraFrame > toCameraFrame( const wse::img1c16_t&      image_in );
WSE_API CameraResult< sCameraFrame > toCameraFrame( const wse::img3c08_t&      image_in );
WSE_API CameraResult< sCameraFrame > toCameraFrame( const wse::img3c08_bgr_t&  image_in );
WSE_API CameraResult< sCameraFrame > toCameraFrame( const wse::img3c16_t&      image_in );
WSE_API CameraResult< sCameraFrame > toCameraFrame( const wse::img3c16_bgr_t&  image_in );
WSE_API CameraResult< sCameraFrame > toCameraFrame( const wse::img4c08_bgra_t& image_in );
//! @}

//!
//! @brief
//!     \~japanese Cameraから1枚読み出し、そのまま`wse::Image_`として返す。
//!     \~english  Reads one frame from a camera and returns it as a `wse::Image_`.
//!
//! @details
//!     \~japanese
//!         `readFrame()`と`toImage()`を続けて呼ぶだけである。Cameraは呼び出し前にStreamingして
//!      @n いる必要がある。読み出しか変換のいずれかが失敗した時点でその失敗をそのまま返し、
//!      @n `image_out`は変更しない。
//!      @n Device制御と画像処理を分けるため、本操作は`WebCamera`のMethodではなく自由関数である。
//!     \~english
//!         Calls `readFrame()` and then `toImage()`. The camera has to be streaming before the call.
//!         The first failure, whether reading or converting, is returned unchanged and `image_out`
//!         is left untouched.
//!      @n This is a free function rather than a `WebCamera` method, so that driving a device and
//!      @n working on its pixels stay apart.
//!
//! @param[in,out] p_camera_inout \~japanese 読み出し元Camera. \~english Camera to read from.
//! @param[out]    p_image_out    \~japanese 読み出したImage.  \~english The image that was read.
//! @param[in]     timeout_ms_in  \~japanese 待機上限.         \~english Timeout.
//!
//! \~
//! @{
WSE_API CameraStatus readImage(
      WebCamera* p_camera_inout, wse::img1c08_t*      p_image_out, std::uint32_t timeout_ms_in );
WSE_API CameraStatus readImage(
      WebCamera* p_camera_inout, wse::img3c08_t*      p_image_out, std::uint32_t timeout_ms_in );
WSE_API CameraStatus readImage(
      WebCamera* p_camera_inout, wse::img3c08_bgr_t*  p_image_out, std::uint32_t timeout_ms_in );
WSE_API CameraStatus readImage(
      WebCamera* p_camera_inout, wse::img4c08_bgra_t* p_image_out, std::uint32_t timeout_ms_in );
//! @}

//!
//! @brief
//!     \~japanese Cameraから指定枚数を読み出し、その平均を`wse::Image_`として返す。
//!     \~english  Reads a requested number of frames and returns their average as a `wse::Image_`.
//!
//! @details
//!     \~japanese
//!         `readAveragedFrame()`と`toImage()`を続けて呼ぶだけである。
//!     \~english
//!         Calls `readAveragedFrame()` and then `toImage()`.
//!
//! @param[in,out] p_camera_inout \~japanese 読み出し元Camera. \~english Camera to read from.
//! @param[out]    p_image_out    \~japanese 平均Image.        \~english The averaged image.
//! @param[in]     count_in       \~japanese 読み出す枚数.     \~english Number of frames to read.
//! @param[in]     timeout_ms_in  \~japanese 1枚あたりの待機上限. \~english Per-frame timeout.
//!
WSE_API CameraStatus readAveragedImage(
      WebCamera*      p_camera_inout
    , wse::img3c08_t* p_image_out
    , std::size_t     count_in
    , std::uint32_t   timeout_ms_in );

} // namespace tmr
} // namespace wse

#endif // WONDERSTEWENGINE_TMR_CAMERA_CAMERAFRAMEOPS_H
