//*****************************************************************************************************************
//!
//! @file    Renderer.h
//! @brief   \~japanese Backend非依存OUI Renderer facadeを定義する.
//! @brief   \~english  Defines the backend-independent OUI renderer facade.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-27, 2026   Create New.
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
#ifndef WONDERSTEWENGINE_OUI_RENDERER_RENDERER_H
#define WONDERSTEWENGINE_OUI_RENDERER_RENDERER_H

#include "../../dynamic.h"
#include "../../wse/data/wse_Image.h"
#include "RendererTypes.h"

#include <cstdint>
#include <memory>

namespace wse
{
namespace oui
{

//! \~japanese Native GPU handleを公開しないRenderer facade.
//! \~english Renderer facade that hides native GPU handles.
//! @details
//! \~japanese Handleは生成したBackendの寿命に属し、Move先へ引き継がれる。別RendererまたはShutdown前のHandleは拒否する。
//! \~english Handles belong to the creating backend lifetime and follow a move. Foreign and pre-shutdown handles are rejected.
class WSE_API Renderer final
{
  private:
    class Impl;
    std::unique_ptr< Impl > m_impl;

  public:
    Renderer();
    ~Renderer();

    Renderer( const Renderer& ) = delete;
    Renderer& operator = ( const Renderer& ) = delete;

    Renderer( Renderer&& source_in ) noexcept;
    Renderer& operator = ( Renderer&& source_in ) noexcept;

    //!
    //! @brief Renderer Backendを初期化する.
    //! @param [in] configuration_in Backend選択、Software adapterおよびValidation設定.
    //! @return \~japanese adapter_nameを含む全設定が同一なら成功。異なる有効設定はAlreadyInitializedで既存状態を保持する。
    //! \~english Success for exactly the same configuration, including adapter_name; a different valid request returns AlreadyInitialized without changing active state.
    //!
    RendererStatus initialize( const sRendererConfiguration& configuration_in );

    //! @brief 全ResourceとBackendを終了する. 未初期化時も安全に呼べる.
    void shutdown() noexcept;

    //! @return \~japanese 初期化済みの場合true. \~english True when initialized.
    bool isInitialized() const noexcept;

    //! @return \~japanese Backend能力またはNotInitialized. \~english Backend capabilities or NotInitialized.
    RendererResult< sRendererCapabilities > getCapabilities() const;

    //!
    //! @brief Active Displayと対応Modeを列挙する.
    //! @return 現在TopologyのDisplay snapshotまたはError. Hotplug後は再列挙が必要.
    //!
    RendererResult< std::vector< sDisplayDescription > > enumerateDisplays() const;

    //!
    //! @brief Textureを生成する.
    //! @param [in] description_in Texture descriptor.
    //! @return Texture handleまたはError.
    //!
    RendererResult< sTextureHandle > createTexture( const sTextureDescription& description_in );

    //!
    //! @brief Textureを解放する.
    //! @param [in] texture_in Texture handle.
    //! @return 解放結果. 既に無効なHandleはResourceNotFound.
    //!
    RendererStatus destroyTexture( const sTextureHandle texture_in );

    //!
    //! @brief CPU FrameをTextureへ非同期Uploadする.
    //! @param [in] texture_in Upload先Texture handle.
    //! @param [in] frame_in   UploadするFrameとLayout.
    //! @return Upload完了を示すFence handleまたはError.
    //!
    RendererResult< sFenceHandle > uploadTexture(
          const sTextureHandle texture_in
        , const sRendererFrame& frame_in
    );

    //!
    //! @brief Coreの`wse::Image_`をTextureへ非同期Uploadする.
    //! @param [in] texture_in Upload先Texture handle.
    //! @param [in] image_in   UploadするImage.
    //! @return Upload完了を示すFence handleまたはError.
    //! @details
    //!     \~japanese
    //!         Imageの型がPixel Formatを選ぶ。Textureの生成時Formatと一致しない場合はError。
    //!      @n `Bgra8Unorm`のTextureへは`img4c08_bgra_t`を渡す。Alphaは4 Channel目として運ばれる。
    //!     \~english
    //!         The image type selects the pixel format, and a texture created with another format is
    //!         an error. Upload an `img4c08_bgra_t` to a `Bgra8Unorm` texture; the alpha travels as
    //!         the fourth channel.
    //!
    //! \~
    //! @{
    RendererResult< sFenceHandle > uploadTexture(
          const sTextureHandle  texture_in
        , const wse::img1c08_t& image_in
    );
    RendererResult< sFenceHandle > uploadTexture(
          const sTextureHandle  texture_in
        , const wse::img4c08_t& image_in
    );
    RendererResult< sFenceHandle > uploadTexture(
          const sTextureHandle       texture_in
        , const wse::img4c08_bgra_t& image_in
    );
    RendererResult< sFenceHandle > uploadTexture(
          const sTextureHandle  texture_in
        , const wse::img4c16_t& image_in
    );
    //! @}

    //!
    //! @brief Meshを生成する.
    //! @param [in] description_in Mesh descriptor.
    //! @return Mesh handleまたはError.
    //!
    RendererResult< sMeshHandle > createMesh( const sMeshDescription& description_in );

    //!
    //! @brief Mesh dataを原子的に更新する.
    //! @param [in] mesh_in        更新するMesh handle.
    //! @param [in] description_in 新しいMesh descriptor.
    //! @return 更新結果. 送信済み描画は更新前のResourceをFence完了まで保持する.
    //!
    RendererStatus updateMesh(
          const sMeshHandle        mesh_in
        , const sMeshDescription& description_in
    );

    //!
    //! @brief Meshを解放する.
    //! @param [in] mesh_in Mesh handle.
    //! @return 解放結果.
    //!
    RendererStatus destroyMesh( const sMeshHandle mesh_in );

    //!
    //! @brief Surfaceを生成する.
    //! @param [in] description_in Surface descriptor.
    //! @return Surface handleまたはError.
    //!
    RendererResult< sSurfaceHandle > createSurface( const sSurfaceDescription& description_in );

    //!
    //! @brief SurfaceのColor textureを取得する.
    //! @param [in] surface_in Surface handle.
    //! @return Surface texture handleまたはError.
    //!
    RendererResult< sTextureHandle > getSurfaceTexture( const sSurfaceHandle surface_in ) const;

    //!
    //! @brief Surfaceの現在状態を取得する.
    //! @param [in] surface_in Surface handle.
    //! @return Surface type、Extent、Window modeおよびDisplay IDまたはError.
    //!
    RendererResult< sSurfaceState > getSurfaceState( const sSurfaceHandle surface_in ) const;

    //!
    //! @brief Windowed SurfaceのClient extentとSwap chainを変更する.
    //! @param [in] surface_in Surface handle.
    //! @param [in] extent_in  新しいClient extent.
    //! @return Resize結果. 成功後はSurface textureを再取得すること.
    //!
    RendererStatus resizeSurface(
          const sSurfaceHandle     surface_in
        , const sRendererExtent2D extent_in
    );

    //!
    //! @brief Windowed／Borderless Fullscreenを切り替える.
    //! @param [in] surface_in Surface handle.
    //! @param [in] request_in Window mode要求.
    //! @return 切替／復元結果. 成功後にExtentとSurface textureを再取得すること.
    //!
    RendererStatus setSurfaceWindowMode(
          const sSurfaceHandle            surface_in
        , const sSurfaceWindowModeRequest& request_in
    );

    //!
    //! @brief OS Eventを処理し、Resize／Display topology変更を返す.
    //! @param [in] surface_in Surface handle.
    //! @return 現在の生存状態、Extent変更およびDisplay topology変更通知またはError.
    //!
    RendererResult< sSurfaceEvents > pollSurfaceEvents( const sSurfaceHandle surface_in );

    //!
    //! @brief Surfaceと所有Textureを解放する.
    //! @param [in] surface_in Surface handle.
    //! @return 解放結果.
    //!
    RendererStatus destroySurface( const sSurfaceHandle surface_in );

    //!
    //! @brief Window Surfaceの保留Eventを処理する.
    //! @param [in] surface_in Window Surface handle.
    //! @return Windowが有効な場合true、閉じられた場合false、またはError.
    //!
    RendererResult< bool > processSurfaceEvents( const sSurfaceHandle surface_in );

    //!
    //! @brief Window Surfaceの現在Bufferを表示する.
    //! @param [in] surface_in Window Surface handle.
    //! @return Present完了を示すFence handleまたはError.
    //!
    RendererResult< sFenceHandle > presentSurface( const sSurfaceHandle surface_in );

    //!
    //! @brief Color-only Render passをCommand queueへ送信する.
    //! @param [in] description_in Render pass descriptor.
    //! @return 送信完了を示すFence handleまたはError.
    //!
    RendererResult< sFenceHandle > executeRenderPass( const sRenderPassDescription& description_in );

    //!
    //! @brief Fence完了を待機する.
    //! @param [in] fence_in      Fence handle.
    //! @param [in] timeout_ms_in Timeout [ms]. ZeroはPoll.
    //! @return 完了、TimeoutまたはError.
    //!
    RendererStatus waitFence( const sFenceHandle fence_in, const std::uint32_t timeout_ms_in );

    //!
    //! @brief TextureをPacked CPU FrameへReadbackする.
    //! @param [in] texture_in    Texture handle.
    //! @param [in] timeout_ms_in Timeout [ms].
    //! @return Packed FrameまたはError.
    //!
    RendererResult< sRendererFrame > readTexture(
          const sTextureHandle texture_in
        , const std::uint32_t  timeout_ms_in
    );

    //!
    //! @brief TextureをCoreの`wse::Image_`へReadbackする.
    //! @param [out] p_image_out   Readback結果. 失敗時は変更しない.
    //! @param [in]  texture_in    Texture handle.
    //! @param [in]  timeout_ms_in Timeout [ms].
    //! @return 成否とError.
    //! @details
    //!     \~japanese
    //!         宛先の型がPixel Formatを選ぶ。Textureが宛先の型へ写らない場合は`UnsupportedFormat`。
    //!      @n `Rgba16Float`はCoreの`Pixel_`が整数型に限られるため対応せず、`sRendererFrame`を
    //!      @n 返すOverloadを使用する。
    //!     \~english
    //!         The destination type selects the pixel format; a texture that does not map onto it
    //!         gives `UnsupportedFormat`.
    //!      @n `Rgba16Float` is unsupported because Core's `Pixel_` is limited to an integer type;
    //!      @n read such a texture with the overload that returns an `sRendererFrame`.
    //!
    //! \~
    //! @{
    RendererStatus readTexture(
          wse::img1c08_t*      p_image_out
        , const sTextureHandle texture_in
        , const std::uint32_t  timeout_ms_in
    );
    RendererStatus readTexture(
          wse::img4c08_t*      p_image_out
        , const sTextureHandle texture_in
        , const std::uint32_t  timeout_ms_in
    );
    RendererStatus readTexture(
          wse::img4c08_bgra_t* p_image_out
        , const sTextureHandle texture_in
        , const std::uint32_t  timeout_ms_in
    );
    RendererStatus readTexture(
          wse::img4c16_t*      p_image_out
        , const sTextureHandle texture_in
        , const std::uint32_t  timeout_ms_in
    );
    //! @}
};

} // namespace oui
} // namespace wse

#endif // WONDERSTEWENGINE_OUI_RENDERER_RENDERER_H
