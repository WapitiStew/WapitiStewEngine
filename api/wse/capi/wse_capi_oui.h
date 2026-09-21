//*****************************************************************************************************************
//!
//! @file    wse_capi_oui.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese OUI Projection描画の平坦C ABI.
//! @brief   \~english  Flat C ABI for OUI projection rendering.
//!
//! @details
//!     \~japanese
//!     @n `wse::oui::renderProjection()`と同じ意味論を公開する。Renderer、Surface、Texture、Mesh、
//!        FenceおよびReadback ObjectはOUI内部の所有のままで、返却するpacked RGBA8 Frameだけが境界を越える。
//!     @n Requestは`wse_capi_projection_request`と`wse_capi_projection_layer`で組み立てる。Layer配列は
//!        呼出中のみ参照し、実装側は保持しない。
//!     @n 結果Frameは`wse_capi_frame_buffer`として返し、呼出元が`wse_capi_frame_buffer_destroy`で解放する。
//!
//!     \~english
//!     @n Exposes the semantics of `wse::oui::renderProjection()`. Renderer, surface, texture, mesh,
//!        fence, and readback objects stay owned inside OUI; only the packed RGBA8 result frame crosses.
//!     @n A request is built from `wse_capi_projection_request` and `wse_capi_projection_layer`. The
//!        layer array is read during the call only and is never retained.
//!     @n The result frame is returned as a `wse_capi_frame_buffer` that the caller releases with
//!        `wse_capi_frame_buffer_destroy`.
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

#ifndef WONDERSTEWENGINE_WSE_CAPI_OUI_H
#define WONDERSTEWENGINE_WSE_CAPI_OUI_H

#include "wse_capi.h"
#include "wse_capi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

//! \~japanese `wse::oui::eRendererBackend`と同じ値のBackend選択.
//! \~english  Backend selection with the same values as `wse::oui::eRendererBackend`.
typedef enum wse_capi_renderer_backend
{
      WSE_CAPI_RENDERER_AUTOMATIC   = 0
    , WSE_CAPI_RENDERER_DIRECT3D12  = 1
    , WSE_CAPI_RENDERER_VULKAN12    = 2
} wse_capi_renderer_backend;

//! \~japanese `wse::oui::eTextureSamplingFilter`と同じ値のSampling filter.
//! \~english  Sampling filter with the same values as `wse::oui::eTextureSamplingFilter`.
typedef enum wse_capi_texture_sampling_filter
{
      WSE_CAPI_SAMPLING_NEAREST = 0
    , WSE_CAPI_SAMPLING_LINEAR  = 1
} wse_capi_texture_sampling_filter;

//! \~japanese `wse::oui::eEdgeBlendCurve`と同じ値のEdge blend curve.
//! \~english  Edge-blend curve with the same values as `wse::oui::eEdgeBlendCurve`.
typedef enum wse_capi_edge_blend_curve
{
      WSE_CAPI_EDGE_BLEND_LINEAR     = 0
    , WSE_CAPI_EDGE_BLEND_SMOOTHSTEP = 1
} wse_capi_edge_blend_curve;

//! \~japanese Projection用2D Vertex. 位置はNDC、UVはTexture座標である.
//! \~english  2D projection vertex: position in NDC and UV in texture space.
typedef struct wse_capi_projection_vertex
{
    float position_x;
    float position_y;
    float texture_u;
    float texture_v;
} wse_capi_projection_vertex;

//! \~japanese Linear RGBA clear color.
//! \~english  Linear RGBA clear color.
typedef struct wse_capi_renderer_color
{
    float red;
    float green;
    float blue;
    float alpha;
} wse_capi_renderer_color;

//! \~japanese Texture UV境界からのEdge blend幅.
//! \~english  Edge-blend widths measured from the texture UV borders.
typedef struct wse_capi_edge_blend
{
    float left;
    float right;
    float top;
    float bottom;
    int32_t curve; //!< \~japanese `wse_capi_edge_blend_curve`. \~english A `wse_capi_edge_blend_curve`.
} wse_capi_edge_blend;

//!
//! \~japanese
//! @brief   1枚のProjection Layer.
//! @details `rgba`は`width * height * 4` byte必須である. `alpha`は任意のpacked R8 mapで、
//!          不要な場合は`NULL`と`0`を渡す. Pointerは呼出中のみ参照する.
//! \~english
//! @brief   One projection layer.
//! @details `rgba` must hold `width * height * 4` bytes. `alpha` is an optional packed R8 map;
//!          pass `NULL` and `0` when it is unused. Pointers are read during the call only.
//!
typedef struct wse_capi_projection_layer
{
    uint32_t width;
    uint32_t height;
    const uint8_t* rgba;
    size_t rgba_size;
    const uint8_t* alpha;
    size_t alpha_size;
    const wse_capi_projection_vertex* vertices;
    size_t vertex_count;
    const uint32_t* indices;
    size_t index_count;
    int32_t sampling_filter; //!< \~japanese `wse_capi_texture_sampling_filter`. \~english A filter value.
    float opacity;
    wse_capi_edge_blend edge_blend;
} wse_capi_projection_layer;

//! \~japanese Projection描画Request. `layers`は呼出中のみ参照する.
//! \~english  Projection render request; `layers` is read during the call only.
typedef struct wse_capi_projection_request
{
    uint32_t output_width;
    uint32_t output_height;
    wse_capi_renderer_color clear_color;
    uint32_t supersample_scale;
    int32_t backend; //!< \~japanese `wse_capi_renderer_backend`. \~english A backend value.
    wse_capi_bool use_software_adapter;
    wse_capi_bool enable_validation;
    //!
    //! \~japanese
    //!     指名するAdapter名の一部。`NULL`または空文字列なら自動選択。呼出中のみ参照する。
    //!  @n 一致するAdapterが無ければ失敗する。黙って別のAdapterで描かないためである。
    //! \~english
    //!     Part of the adapter name to select; `NULL` or an empty string selects automatically, and
    //!     the string is read during the call only.
    //!  @n No match is a failure rather than a quiet render on some other adapter.
    //!
    const char* adapter_name;
    uint32_t timeout_milliseconds;
    const wse_capi_projection_layer* layers;
    size_t layer_count;
} wse_capi_projection_request;

//! \~japanese 描画結果Frameの寸法. Byte列は別のFrame Buffer Handleが所有する.
//! \~english  Dimensions of a rendered frame; the bytes are owned by a separate frame-buffer handle.
//! \~japanese Adapter名を収める領域の大きさ. Vulkanの上限に合わせている.
//! \~english  Size of the room an adapter name gets, taken from the Vulkan limit.
#define WSE_CAPI_ADAPTER_NAME_CAPACITY 256U

typedef struct wse_capi_projection_frame
{
    uint32_t width;
    uint32_t height;
    size_t row_pitch;
    //!
    //! \~japanese
    //!     実際に描いたAdapterの名前. NUL終端のUTF-8.
    //!  @n この構造体は値だけを運ぶため、Handleを介さず固定長で持つ。
    //!  @n 領域を超える名前は切り詰めるが、終端は必ず書く。
    //! \~english
    //!     Name of the adapter that actually drew the frame, as NUL-terminated UTF-8.
    //!  @n This structure carries values rather than handles, so the name is held inline.
    //!  @n A longer name is truncated, and the terminator is always written.
    //!
    char adapter_name[ WSE_CAPI_ADAPTER_NAME_CAPACITY ];
} wse_capi_projection_frame;

//!
//! \~japanese
//! @brief   Projectionを1回描画する.
//! @param [out] p_frame_out  結果Frameの寸法. `NULL`不可.
//! @param [out] p_data_out   packed RGBA8 Byte列を所有するHandle. `NULL`不可.
//!                         成功時のみ設定し、呼出元が`wse_capi_frame_buffer_destroy`で解放する.
//! @param [in]  request_in   描画するProjectionの指定. `NULL`不可.
//! \~english
//! @brief   Renders one projection.
//! @param [out] p_frame_out  Result frame dimensions; must not be `NULL`.
//! @param [out] p_data_out   Handle owning the packed RGBA8 bytes; must not be `NULL`. It is set on
//!                         success only and released by the caller with `wse_capi_frame_buffer_destroy`.
//! @param [in]  request_in   Projection to render; must not be `NULL`.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_render_projection(
      wse_capi_projection_frame*         p_frame_out
    , wse_capi_frame_buffer*             p_data_out
    , const wse_capi_projection_request* request_in );

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WONDERSTEWENGINE_WSE_CAPI_OUI_H
