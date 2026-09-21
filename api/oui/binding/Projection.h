//*****************************************************************************************************************
//!
//! @file    Projection.h
//! @brief   \~japanese 言語Binding向けの高水準・Handle非公開のProjection契約.
//! @brief   \~english  High-level, handle-free Projection contract for language bindings.
//!
//! @date
//!   Aug-29, 2026   Create New.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_OUI_BINDING_PROJECTION_H
#define WONDERSTEWENGINE_OUI_BINDING_PROJECTION_H

#include <utility>
#include "../../dynamic.h"
#include "../../wse/binding/FrameBuffer.h"
#include "../renderer/ProjectionPipeline.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wse
{
namespace oui
{

//! \~japanese Managed言語から指定できる所有Image／Mesh Layer. Native handleは公開しない.
//! \~english  An owned image/mesh layer for managed languages; it exposes no native handles.
struct sProjectionImageLayer
{
    std::uint32_t                  width;
    std::uint32_t                  height;
    std::vector<std::uint8_t>      rgba;
    std::vector<std::uint8_t>      alpha; //!< Optional packed R8 alpha map.
    std::vector<sRendererVertex2D> vertices;
    std::vector<std::uint32_t>     indices;
    eTextureSamplingFilter         sampling_filter;
    float                          opacity;
    sEdgeBlendDescription          edge_blend;

    //! @brief Construct all members with explicit defaults.
    sProjectionImageLayer(
          std::uint32_t width_in = 0U
        , std::uint32_t height_in = 0U
        , const std::vector<std::uint8_t>& rgba_in = {}
        , const std::vector<std::uint8_t>& alpha_in = {}
        , const std::vector<sRendererVertex2D>& vertices_in = {}
        , const std::vector<std::uint32_t>& indices_in = {}
        , eTextureSamplingFilter sampling_filter_in = eTextureSamplingFilter::Linear
        , float opacity_in = 1.0F
        , const sEdgeBlendDescription& edge_blend_in = {}
    )
        : width           ( width_in )
        , height          ( height_in )
        , rgba            ( rgba_in )
        , alpha           ( alpha_in )
        , vertices        ( vertices_in )
        , indices         ( indices_in )
        , sampling_filter ( sampling_filter_in )
        , opacity         ( opacity_in )
        , edge_blend      ( edge_blend_in )
    {
    }
};

//! \~japanese 1回の同期Projection要求. Backend resource lifetimeは呼出し内で完結する.
//! \~english  A one-shot synchronous Projection request whose backend resources remain internal.
struct sProjectionRenderRequest
{
    std::uint32_t                  output_width;
    std::uint32_t                  output_height;
    sRendererColor                clear_color;
    std::uint32_t                 supersample_scale;
    eRendererBackend              backend;
    bool                          use_software_adapter;
    bool                          enable_validation;
    //!
    //! \~japanese
    //!     指名するAdapter名の一部。空なら自動選択。
    //!  @n 一致するAdapterが無ければ失敗する。黙って別のAdapterで描かないためである。
    //! \~english
    //!     Part of the adapter name to select; empty selects automatically.
    //!  @n No match is a failure rather than a quiet render on some other adapter.
    //!
    std::string                   adapter_name;
    std::uint32_t                 timeout_ms;
    std::vector<sProjectionImageLayer> layers;

    //! @brief Construct all members with explicit defaults.
    sProjectionRenderRequest(
          std::uint32_t output_width_in = 0U
        , std::uint32_t output_height_in = 0U
        , const sRendererColor& clear_color_in = {}
        , std::uint32_t supersample_scale_in = 1U
        , eRendererBackend backend_in = eRendererBackend::Automatic
        , bool use_software_adapter_in = false
        , bool enable_validation_in = false
        , const std::string& adapter_name_in = {}
        , std::uint32_t timeout_ms_in = 30000U
        , const std::vector<sProjectionImageLayer>& layers_in = {}
    )
        : output_width         ( output_width_in )
        , output_height        ( output_height_in )
        , clear_color          ( clear_color_in )
        , supersample_scale    ( supersample_scale_in )
        , backend              ( backend_in )
        , use_software_adapter ( use_software_adapter_in )
        , enable_validation    ( enable_validation_in )
        , adapter_name         ( adapter_name_in )
        , timeout_ms           ( timeout_ms_in )
        , layers               ( layers_in )
    {
    }
};

//! \~japanese Packed RGBA8 Projection結果. \~english Packed RGBA8 Projection result.
struct sProjectionRenderFrame
{
    std::uint32_t             width;
    std::uint32_t             height;
    std::size_t               row_pitch;
    //!
    //! \~japanese
    //!     実際に描いたAdapterの名前。
    //!  @n Adapterを指名しなかった場合にどれが選ばれたかは、ここでしか分からない。
    //! \~english
    //!     Name of the adapter that actually drew the frame.
    //!  @n When no adapter was named, this is the only place the choice is reported.
    //!
    std::string               adapter_name;
    wse::binding::FrameBuffer data;

    //! @brief Construct all members with explicit defaults.
    sProjectionRenderFrame(
          std::uint32_t width_in = 0U
        , std::uint32_t height_in = 0U
        , std::size_t row_pitch_in = 0U
        , const std::string& adapter_name_in = {}
        , const wse::binding::FrameBuffer& data_in = {}
    )
        : width        ( width_in )
        , height       ( height_in )
        , row_pitch    ( row_pitch_in )
        , adapter_name ( adapter_name_in )
        , data         ( data_in )
    {
    }
};

//! \~japanese Projection要求を実行し、所有RGBA8 Bufferを返す. GPU handle／Fenceは外部へ公開しない.
//! \~english  Executes a Projection request and returns owned RGBA8 bytes without exposing GPU handles or fences.
WSE_API RendererResult<sProjectionRenderFrame> renderProjection(
    const sProjectionRenderRequest& request_in );

} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_OUI_BINDING_PROJECTION_H
