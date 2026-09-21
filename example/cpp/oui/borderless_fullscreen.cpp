// @file borderless_fullscreen.cpp
// @brief Switches a window surface to borderless fullscreen on the primary display and
//        presents 60 generated frames there, then restores the windowed mode.
//
// Requires WSE_BUILD_OUI=ON and a desktop session with an active display. The transition
// replaces the swap chain, so the back-buffer texture handle must be re-acquired after a
// successful mode change.
//
// Two exits return 0 without drawing anything: an environment whose capabilities do not
// include borderless fullscreen, and one with no renderer-compatible display. Both are the
// normal answer on a headless agent or behind a software adapter, so they are reported and
// accepted rather than treated as errors; a genuine failure goes through fail() and returns 1.
//
// The renderer is not thread safe. Every call below runs on this one thread, and that is a
// requirement of the API rather than a simplification of the sample.

#include <oui/renderer/Renderer.h>
#include <wse/stew.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr std::uint32_t WINDOW_WIDTH = 960U;
constexpr std::uint32_t WINDOW_HEIGHT = 540U;
constexpr std::uint32_t SOURCE_WIDTH = 640U;
constexpr std::uint32_t SOURCE_HEIGHT = 360U;
//! Twice the windowed sample's length so the fullscreen state is easy to observe.
constexpr int FRAME_COUNT = 60;
constexpr std::uint32_t FENCE_TIMEOUT_MS = 30000U;
constexpr std::chrono::milliseconds FRAME_INTERVAL( 33 );

int fail( const std::string& operation_in, const wse::oui::RendererError& error_in )
{
    wse::WLog() << "ERROR:" << operation_in << "failed."
                << "category=" << static_cast< int >( error_in.category() )
                << "code=" << static_cast< int >( error_in.code() )
                << "message=" << error_in.message();
    return 1;
}

//! One generated frame: a diagonally scrolling gradient with a bright sweeping bar, so
//! motion stays obvious on the full screen.
void paintFrame( wse::img4c08_t* const p_image_inout, const int frame_in )
{
    wse::img4c08_t& image_inout = *p_image_inout;

    const std::uint32_t bar_width = SOURCE_WIDTH / 12U;
    const std::uint32_t bar_left = static_cast< std::uint32_t >( frame_in )
        * ( SOURCE_WIDTH - bar_width ) / ( FRAME_COUNT - 1 );
    for ( std::uint32_t y = 0; y < SOURCE_HEIGHT; ++y )
    {
        for ( std::uint32_t x = 0; x < SOURCE_WIDTH; ++x )
        {
            const bool inside_bar = x >= bar_left && x < bar_left + bar_width;
            auto& pixel = image_inout[ y ][ x ];
            pixel[ 0U ] = inside_bar ? 255U : static_cast< std::uint8_t >( x + frame_in * 6 );
            pixel[ 1U ] = inside_bar ? 255U : static_cast< std::uint8_t >( y + frame_in * 3 );
            pixel[ 2U ] = inside_bar ? 255U : 96U;
            pixel[ 3U ] = 255U;
        }
    }
}

} // namespace

int main()
{
    using namespace wse::oui;

    wse::registDefaultLog();

    Renderer renderer;
    sRendererConfiguration configuration;
    configuration.backend = eRendererBackend::Automatic;
    auto initialize_result = renderer.initialize( configuration );
    if ( !initialize_result.succeeded() )
    {
        wse::WLog() << "hardware adapter unavailable; retrying with the software adapter";
        renderer.shutdown();
        configuration.use_software_adapter = true;
        initialize_result = renderer.initialize( configuration );
    }
    if ( !initialize_result.succeeded() )
        return fail( "renderer initialization", initialize_result.error() );
    const auto capabilities = renderer.getCapabilities();
    if ( !capabilities.succeeded() )
        return fail( "capability query", capabilities.error() );
    if ( !capabilities.value().supports_window ||
         !capabilities.value().supports_borderless_fullscreen ||
         !capabilities.value().supports_display_enumeration )
    {
        wse::WLog() << "this environment does not support borderless fullscreen; nothing to show";
        return 0;
    }
    wse::WLog() << "renderer ready, adapter=" << capabilities.value().adapter_name;

    // The surface starts as a normal window; fullscreen is a mode change, not a rebuild.
    sSurfaceDescription surface_description;
    surface_description.type = eSurfaceType::Window;
    surface_description.extent = { WINDOW_WIDTH, WINDOW_HEIGHT };
    surface_description.format = eRendererPixelFormat::Rgba8Unorm;
    surface_description.buffer_count = 2U;
    surface_description.vertical_sync = true;
    surface_description.visible = true;
    surface_description.title = "WSE borderless fullscreen example";
    const auto surface = renderer.createSurface( surface_description );
    if ( !surface.succeeded() )
        return fail( "window surface creation", surface.error() );
    renderer.processSurfaceEvents( surface.value() );

    // Pick the primary renderer-compatible display; fall back to any compatible one.
    const auto displays = renderer.enumerateDisplays();
    if ( !displays.succeeded() )
        return fail( "display enumeration", displays.error() );
    const sDisplayDescription* p_target = nullptr;
    for ( const auto& display : displays.value() )
    {
        wse::WLog() << "display" << display.id << ":" << display.display_name
                    << display.desktop_extent.width << "x" << display.desktop_extent.height
                    << ( display.primary ? "primary" : "" )
                    << ( display.renderer_compatible ? "compatible" : "" );
        if ( !display.renderer_compatible ) continue;
        if ( p_target == nullptr || display.primary ) p_target = &display;
    }
    if ( p_target == nullptr )
    {
        wse::WLog() << "no renderer-compatible display is active; nothing to show";
        renderer.destroySurface( surface.value() );
        renderer.shutdown();
        return 0;
    }

    // The borderless transition needs only the display ID; no display-mode change occurs.
    sSurfaceWindowModeRequest fullscreen_request;
    fullscreen_request.mode = eSurfaceWindowMode::BorderlessFullscreen;
    fullscreen_request.display_id = p_target->id;
    const auto fullscreen_result =
        renderer.setSurfaceWindowMode( surface.value(), fullscreen_request );
    if ( !fullscreen_result.succeeded() )
        return fail( "borderless fullscreen transition", fullscreen_result.error() );
    const auto state = renderer.getSurfaceState( surface.value() );
    if ( !state.succeeded() )
        return fail( "surface state query", state.error() );
    wse::WLog() << "entered borderless fullscreen: display=" << state.value().display_id
                << "extent=" << state.value().extent.width << "x"
                << state.value().extent.height;

    sTextureDescription source_description;
    source_description.extent = { SOURCE_WIDTH, SOURCE_HEIGHT };
    source_description.format = eRendererPixelFormat::Rgba8Unorm;
    source_description.usage = eTextureUsage::Sampled | eTextureUsage::TransferDestination;
    source_description.initial_state = eTextureState::CopyDestination;
    const auto source = renderer.createTexture( source_description );
    if ( !source.succeeded() )
        return fail( "source texture creation", source.error() );

    sMeshDescription quad_description;
    quad_description.vertices = {
          { -1.0F,  1.0F, 0.0F, 0.0F }
        , {  1.0F,  1.0F, 1.0F, 0.0F }
        , {  1.0F, -1.0F, 1.0F, 1.0F }
        , { -1.0F, -1.0F, 0.0F, 1.0F }
    };
    quad_description.indices = { 0U, 1U, 2U, 0U, 2U, 3U };
    const auto quad = renderer.createMesh( quad_description );
    if ( !quad.succeeded() )
        return fail( "quad mesh creation", quad.error() );

    // One reused RGBA image is the scratch buffer the sample video paints into.
    wse::img4c08_t source_image( SOURCE_WIDTH, SOURCE_HEIGHT );

    int presented = 0;
    for ( int frame_index = 0; frame_index < FRAME_COUNT; ++frame_index )
    {
        const auto events = renderer.processSurfaceEvents( surface.value() );
        if ( !events.succeeded() )
            return fail( "window event processing", events.error() );
        if ( !events.value() )
        {
            wse::WLog() << "window closed by the user";
            break;
        }

        paintFrame( &source_image, frame_index );
        // uploadTexture() takes the Core image directly; the image type names the format.
        const auto upload = renderer.uploadTexture( source.value(), source_image );
        if ( !upload.succeeded() ||
             !renderer.waitFence( upload.value(), FENCE_TIMEOUT_MS ).succeeded() )
            return fail( "source upload", upload.error() );

        // A successful transition replaced the swap chain, so the back buffer is looked
        // up fresh every frame; the empty render_area always covers the full extent.
        const auto backbuffer = renderer.getSurfaceTexture( surface.value() );
        if ( !backbuffer.succeeded() )
            return fail( "back-buffer lookup", backbuffer.error() );
        sRenderPassDescription window_pass;
        window_pass.color_attachment = backbuffer.value();
        window_pass.clear_color = { 0.0F, 0.0F, 0.0F, 1.0F };
        window_pass.draw_commands.push_back( sMeshDrawCommand{
              quad.value()
            , source.value()
            , {}
            , eTextureSamplingFilter::Linear
            , eColorBlendMode::Replace
            , 1.0F
            , {}
        } );
        window_pass.final_state = eTextureState::Present;
        const auto pass_result = renderer.executeRenderPass( window_pass );
        if ( !pass_result.succeeded() )
            return fail( "window pass", pass_result.error() );

        const auto present_result = renderer.presentSurface( surface.value() );
        if ( !present_result.succeeded() )
            return fail( "present", present_result.error() );
        if ( !renderer.waitFence( present_result.value(), FENCE_TIMEOUT_MS ).succeeded() )
            return fail( "present fence wait", present_result.error() );
        ++presented;
        std::this_thread::sleep_for( FRAME_INTERVAL );
    }

    // Leave the desktop the way the sample found it before tearing anything down.
    const auto windowed_result =
        renderer.setSurfaceWindowMode( surface.value(), sSurfaceWindowModeRequest{} );
    if ( windowed_result.succeeded() )
    {
        wse::WLog() << "restored the windowed mode";
    }
    else
    {
        wse::WLog() << "windowed-mode restore failed:" << windowed_result.error().message();
    }
    wse::WLog() << "presented frames:" << presented << "of" << FRAME_COUNT;

    renderer.destroyMesh( quad.value() );
    renderer.destroyTexture( source.value() );
    renderer.destroySurface( surface.value() );
    renderer.shutdown();
    return 0;
}
