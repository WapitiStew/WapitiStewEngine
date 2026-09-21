//*****************************************************************************************************************
//!
//! @file    RendererFrameOps.cpp
//! @brief   \~japanese Renderer FrameとCore Imageの相互変換を実装する.
//! @brief   \~english  Implements the conversion between a renderer frame and a Core image.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Sep-06, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#include "../../../api/oui/renderer/RendererFrameOps.h"

#include <new>
#include <utility>

namespace wse
{
namespace oui
{
namespace
{

template< typename T >
RendererResult< T > frameOpsFailure(
      const eRendererErrorCategory category_in
    , const eRendererErrorCode     code_in
    , const char* const            message_in )
{
    return RendererResult< T >::failure( RendererError( category_in, code_in, message_in ) );
}

//! \~japanese Frameを検証し、Coreへ渡すViewを組み立てる. \~english Validates a frame and builds the Core view.
template< wse::ePixFormat _Pf >
RendererStatus buildInterleavedView(
      wse::sInterleavedView* const p_view_out
    , const sRendererFrame&       frame_in )
{
    wse::sInterleavedView& view_out = *p_view_out;

    wse::ePixFormat mapped = wse::ePixFormat::CH1D8;
    if( !coreFormatOf( &mapped, frame_in.description.format ) || mapped != _Pf )
    {
        return frameOpsFailure< void >(
            eRendererErrorCategory::Unsupported, eRendererErrorCode::UnsupportedFormat,
            "This renderer pixel format does not map onto the requested image format." );
    }

    const RendererError validation = validateRendererFrame( frame_in );
    if( !validation.ok() )
    {
        return RendererStatus::failure( validation );
    }

    view_out.data             = frame_in.data.data();
    view_out.width            = frame_in.description.extent.width;
    view_out.height           = frame_in.description.extent.height;
    view_out.row_stride       = frame_in.description.effectiveRowPitch();
    view_out.accessible_bytes = frame_in.data.size();
    if( !wse::isInterleavedViewValid< _Pf >( view_out ) )
    {
        return frameOpsFailure< void >(
            eRendererErrorCategory::Validation, eRendererErrorCode::InvalidDescription,
            "The renderer frame layout does not fit inside its own data." );
    }
    return RendererStatus::success();
}

//! \~japanese 取り込みの共通形. Coreの例外は構造化Errorへ畳む.
//! \~english  Shared conversion body; a Core exception is folded into a structured error.
template< wse::ePixFormat _Pf >
RendererStatus convertFrameToImage( wse::Image_< _Pf >* const p_image_out, const sRendererFrame& frame_in )
{
    wse::Image_< _Pf >& image_out = *p_image_out;

    wse::sInterleavedView view;
    const RendererStatus prepared = buildInterleavedView< _Pf >( &view, frame_in );
    if( !prepared.succeeded() )
    {
        return prepared;
    }
    try
    {
        image_out = wse::makeImageFromInterleaved< _Pf >( view );
    }
    catch( const std::exception& )
    {
        return frameOpsFailure< void >(
            eRendererErrorCategory::Resource, eRendererErrorCode::ResourceExhausted,
            "Allocating the image for this renderer frame failed." );
    }

    return RendererStatus::success();
}

//! \~japanese 書き出しの共通形. \~english Shared body that writes an image out as a frame.
template< wse::ePixFormat _Pf >
RendererResult< sRendererFrame > convertImageToFrame(
      const wse::Image_< _Pf >&  image_in
    , const eRendererPixelFormat format_in )
{
    if( image_in.width() == 0U || image_in.height() == 0U )
    {
        return frameOpsFailure< sRendererFrame >(
            eRendererErrorCategory::Validation, eRendererErrorCode::InvalidArgument,
            "An empty image has no renderer frame." );
    }

    const std::size_t row_bytes = wse::packedRowBytes< _Pf >( image_in.width() );
    if( row_bytes == 0U )
    {
        return frameOpsFailure< sRendererFrame >(
            eRendererErrorCategory::Validation, eRendererErrorCode::InvalidArgument,
            "The image extent overflows a packed renderer frame." );
    }

    sRendererFrame frame;
    frame.description.extent.width  = static_cast< std::uint32_t >( image_in.width() );
    frame.description.extent.height = static_cast< std::uint32_t >( image_in.height() );
    frame.description.format        = format_in;
    // Zero selects the minimum packed pitch, so the backend keeps its own alignment rule.
    frame.description.row_pitch     = 0U;
    try
    {
        frame.data.resize( row_bytes * image_in.height() );
    }
    catch( const std::bad_alloc& )
    {
        return frameOpsFailure< sRendererFrame >(
            eRendererErrorCategory::Resource, eRendererErrorCode::ResourceExhausted,
            "Allocating the renderer frame for this image failed." );
    }

    wse::sInterleavedTarget target;
    target.data             = frame.data.data();
    target.width            = image_in.width();
    target.height           = image_in.height();
    target.row_stride       = row_bytes;
    target.accessible_bytes = frame.data.size();
    if( !wse::writeImageToInterleaved( &target, image_in ) )
    {
        return frameOpsFailure< sRendererFrame >(
            eRendererErrorCategory::Validation, eRendererErrorCode::InvalidDescription,
            "Writing the image into a renderer frame failed." );
    }
    return RendererResult< sRendererFrame >::success( frame );
}

} // namespace


bool coreFormatOf( wse::ePixFormat* const p_format_out, const eRendererPixelFormat format_in ) noexcept
{
    wse::ePixFormat& format_out = *p_format_out;

    switch( format_in )
    {
        case eRendererPixelFormat::R8Unorm     : format_out = wse::ePixFormat::CH1D8;   return true;
        case eRendererPixelFormat::Rgba8Unorm  : format_out = wse::ePixFormat::CH4D8;   return true;
        case eRendererPixelFormat::Bgra8Unorm  : format_out = wse::ePixFormat::BGRA4D8; return true;
        case eRendererPixelFormat::Rgba16Unorm : format_out = wse::ePixFormat::CH4D16;  return true;
        // Rgba16Floatは半精度浮動小数点であり、Coreの Pixel_ は整数型に限られる。
        default                                : return false;
    }
}


RendererStatus toImage( wse::img1c08_t* const p_image_out, const sRendererFrame& frame_in )
{
    return convertFrameToImage< wse::ePixFormat::CH1D8 >( p_image_out, frame_in );
}

RendererStatus toImage( wse::img4c08_t* const p_image_out, const sRendererFrame& frame_in )
{
    return convertFrameToImage< wse::ePixFormat::CH4D8 >( p_image_out, frame_in );
}

RendererStatus toImage( wse::img4c08_bgra_t* const p_image_out, const sRendererFrame& frame_in )
{
    return convertFrameToImage< wse::ePixFormat::BGRA4D8 >( p_image_out, frame_in );
}

RendererStatus toImage( wse::img4c16_t* const p_image_out, const sRendererFrame& frame_in )
{
    return convertFrameToImage< wse::ePixFormat::CH4D16 >( p_image_out, frame_in );
}


RendererResult< sRendererFrame > toRendererFrame( const wse::img1c08_t& image_in )
{
    return convertImageToFrame( image_in, eRendererPixelFormat::R8Unorm );
}

RendererResult< sRendererFrame > toRendererFrame( const wse::img4c08_t& image_in )
{
    return convertImageToFrame( image_in, eRendererPixelFormat::Rgba8Unorm );
}

RendererResult< sRendererFrame > toRendererFrame( const wse::img4c08_bgra_t& image_in )
{
    return convertImageToFrame( image_in, eRendererPixelFormat::Bgra8Unorm );
}

RendererResult< sRendererFrame > toRendererFrame( const wse::img4c16_t& image_in )
{
    return convertImageToFrame( image_in, eRendererPixelFormat::Rgba16Unorm );
}

} // namespace oui
} // namespace wse
