//*****************************************************************************************************************
//!
//! @file    oui_renderer_lifetime_contract.cpp
//! @brief   \~japanese Portable RendererのResource所有と再初期化契約を固定する.
//! @brief   \~english  Fixes the portable renderer's resource ownership and reinitialization contract.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Sep-02, 2026   Create New.
//!
//! @details
//!     \~japanese
//!         Legacy `InterfaceGPU`のLifetime契約を固定していた`wse.oui.gpu_lifetime`に対応する、
//!      @n Portable API側の契約である。LegacyはDevice Pointerの共有と解放を観測していたが、Portableは
//!      @n 生Handleを公開しないため、観測点はInitialize状態、Resource Handleの有効性、およびShutdown後の
//!      @n 操作拒否になる。両者が同じ性質を別の表現で保証していることを示す。
//!     \~english
//!         The portable counterpart of `wse.oui.gpu_lifetime`, which fixed the legacy `InterfaceGPU`
//!         lifetime contract. The legacy test observed a shared device pointer being released; the
//!         portable API publishes no raw handle, so the observable points are the initialization
//!         state, resource-handle validity, and the refusal of operations after shutdown. The two
//!         express the same property in different vocabulary.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************
#include <oui/renderer/Renderer.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr std::uint32_t EXTENT     = 4U;
constexpr std::uint32_t TIMEOUT_MS = 30000U;

int g_failures = 0;

void expect( const bool condition_in, const std::string& message_in )
{
    if( !condition_in )
    {
        std::cerr << "FAILED: " << message_in << '\n';
        ++g_failures;
    }
}

wse::oui::sRendererConfiguration defaultConfiguration()
{
    wse::oui::sRendererConfiguration configuration;
#if defined( _WIN32 )
    configuration.backend = wse::oui::eRendererBackend::Direct3D12;
#else
    configuration.backend = wse::oui::eRendererBackend::Vulkan12;
#endif
    return configuration;
}

wse::oui::sTextureDescription sampledTexture()
{
    wse::oui::sTextureDescription description;
    description.extent = { EXTENT, EXTENT };
    description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    description.usage  = static_cast< wse::oui::eTextureUsage >(
          static_cast< std::uint16_t >( wse::oui::eTextureUsage::Sampled )
        | static_cast< std::uint16_t >( wse::oui::eTextureUsage::TransferDestination ) );
    return description;
}

} // namespace


int main()
{
    wse::oui::Renderer renderer;
    expect( !renderer.isInitialized(), "A default renderer is uninitialized" );

    // 初期化前のOperationはCrashせず、Lifecycle失敗として拒否される。
    expect( !renderer.createTexture( sampledTexture() ).succeeded(),
        "createTexture before initialize must fail" );
    expect( !renderer.getCapabilities().succeeded(),
        "getCapabilities before initialize must fail" );

    if( !renderer.initialize( defaultConfiguration() ).succeeded() )
    {
        std::cerr << "FAILED: the renderer could not initialize\n";
        return 1;
    }
    expect( renderer.isInitialized(), "Initialization state matches the result" );

    const auto capabilities = renderer.getCapabilities();
    expect( capabilities.succeeded(), "An initialized renderer reports its capabilities" );
    expect( capabilities.succeeded() && !capabilities.value().adapter_name.empty(),
        "An initialized renderer names the adapter it selected" );

    const auto texture = renderer.createTexture( sampledTexture() );
    expect( texture.succeeded(), "An initialized renderer creates a texture" );

    // Legacyが device() の共有を観測していた箇所を、Handleの有効性で観測する。
    expect( renderer.getSurfaceState( {} ).succeeded() == false,
        "An unknown surface handle is refused rather than dereferenced" );
    expect( !renderer.destroyTexture( {} ).succeeded(),
        "Destroying an unknown texture handle is refused" );

    if( texture.succeeded() )
    {
        expect( renderer.destroyTexture( texture.value() ).succeeded(),
            "A live texture handle is destroyed" );
        // 二重解放はCrashせず、Resourceが見つからないという明示的な失敗になる。
        expect( !renderer.destroyTexture( texture.value() ).succeeded(),
            "Destroying the same texture twice is refused" );
    }

    renderer.shutdown();
    expect( !renderer.isInitialized(), "shutdown clears the initialization state" );
    renderer.shutdown();
    expect( !renderer.isInitialized(), "shutdown is idempotent" );

    // Shutdown後のOperationは、初期化前と同じ扱いで拒否される。
    expect( !renderer.createTexture( sampledTexture() ).succeeded(),
        "createTexture after shutdown must fail" );
    expect( !renderer.getCapabilities().succeeded(),
        "getCapabilities after shutdown must fail" );

    // Legacyが Release 後の再initializeを許したのと同じく、Portableも再初期化できる。
    expect( renderer.initialize( defaultConfiguration() ).succeeded(),
        "A shut-down renderer initializes again" );
    expect( renderer.isInitialized(), "Reinitialization restores the initialization state" );

    const auto reused = renderer.createTexture( sampledTexture() );
    expect( reused.succeeded(), "A reinitialized renderer creates a texture again" );
    if( reused.succeeded() )
    {
        // This checks only the empty sentinel. Handles from an earlier renderer lifetime
        // must be discarded by callers; this assertion does not prove epoch isolation.
        expect( !renderer.readTexture( {}, TIMEOUT_MS ).succeeded(),
            "An empty handle stays invalid after reinitialization" );
        renderer.destroyTexture( reused.value() );
    }
    renderer.shutdown();

    if( g_failures == 0 )
    {
        std::cout << "oui renderer lifetime contract passed\n";
        return 0;
    }
    std::cerr << g_failures << " renderer lifetime expectation(s) failed\n";
    return 1;
}
