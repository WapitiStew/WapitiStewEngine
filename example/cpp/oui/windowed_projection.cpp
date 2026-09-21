// @file windowed_projection.cpp
// @brief Renders 30 frames of three generated sample videos through the full projection
//        chain and presents them in a window:
//        source textures -> (warp + alpha map) -> screen texture
//                        -> (keystone warp + alpha map) -> projection texture -> window.
//
// Requires WSE_BUILD_OUI=ON and a desktop session that can show a window. Sample videos
// are generated procedurally; the repository intentionally ships no image assets.

#include <oui/renderer/ProjectionPipeline.h>
#include <oui/renderer/ProjectionMeshAdapter.h>
#include <wse/stew.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr std::uint32_t SOURCE_WIDTH = 256U;
constexpr std::uint32_t SOURCE_HEIGHT = 256U;
constexpr std::uint32_t SCREEN_WIDTH = 960U;
constexpr std::uint32_t SCREEN_HEIGHT = 540U;
constexpr std::uint32_t PROJECTION_WIDTH = 960U;
constexpr std::uint32_t PROJECTION_HEIGHT = 540U;
constexpr int FRAME_COUNT = 30;
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

//! Creates a CPU-uploadable sampled texture (a video source or an alpha map).
wse::oui::sTextureDescription uploadableDescription(
      const wse::oui::sRendererExtent2D    extent_in
    , const wse::oui::eRendererPixelFormat format_in )
{
    wse::oui::sTextureDescription description;
    description.extent = extent_in;
    description.format = format_in;
    description.usage = wse::oui::eTextureUsage::Sampled |
        wse::oui::eTextureUsage::TransferDestination;
    description.initial_state = wse::oui::eTextureState::CopyDestination;
    return description;
}

//! Creates an intermediate target that is rendered to and then sampled by the next pass.
wse::oui::sTextureDescription renderTargetDescription(
    const wse::oui::sRendererExtent2D extent_in )
{
    wse::oui::sTextureDescription description;
    description.extent = extent_in;
    description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
    description.usage = wse::oui::eTextureUsage::RenderTarget |
        wse::oui::eTextureUsage::Sampled;
    description.initial_state = wse::oui::eTextureState::RenderTarget;
    return description;
}

//! Builds a single-channel alpha map that fades to transparent inside a border band.
//! An R8 alpha texture contributes its red channel as the per-pixel alpha factor, and a
//! one-channel Core image is exactly that layout.
wse::img1c08_t makeEdgeFadeAlpha( const wse::oui::sRendererExtent2D extent_in )
{
    wse::img1c08_t image( extent_in.width, extent_in.height );
    const double fade = 0.12 * std::min( extent_in.width, extent_in.height );
    for ( std::uint32_t y = 0; y < extent_in.height; ++y )
    {
        for ( std::uint32_t x = 0; x < extent_in.width; ++x )
        {
            const double distance = std::min(
                std::min< double >( x, extent_in.width - 1U - x ),
                std::min< double >( y, extent_in.height - 1U - y ) );
            const double factor = std::min( 1.0, distance / fade );
            image[ y ][ x ][ 0U ] = static_cast< std::uint8_t >( factor * 255.0 );
        }
    }
    return image;
}

//! Converts a source-rectangle-to-target-rectangle mapping into a portable mesh through
//! the legacy pixel-space mesh adapter.
wse::oui::RendererResult< wse::oui::sMeshHandle > createRegionMesh(
      wse::oui::Renderer*                p_renderer_inout
    , const wse::oui::sRendererExtent2D  source_extent_in
    , const wse::float64_rect&           target_pixels_in
    , const wse::oui::sRendererExtent2D  target_extent_in )
{
    wse::float64_mesh2d mesh( 2, 2 );
    const double source_right = static_cast< double >( source_extent_in.width - 1U );
    const double source_bottom = static_cast< double >( source_extent_in.height - 1U );
    mesh[ 0 ][ 0 ].src = { 0.0, 0.0 };
    mesh[ 0 ][ 1 ].src = { source_right, 0.0 };
    mesh[ 1 ][ 0 ].src = { 0.0, source_bottom };
    mesh[ 1 ][ 1 ].src = { source_right, source_bottom };
    mesh[ 0 ][ 0 ].dst = { target_pixels_in.min_x(), target_pixels_in.min_y() };
    mesh[ 0 ][ 1 ].dst = { target_pixels_in.max_x(), target_pixels_in.min_y() };
    mesh[ 1 ][ 0 ].dst = { target_pixels_in.min_x(), target_pixels_in.max_y() };
    mesh[ 1 ][ 1 ].dst = { target_pixels_in.max_x(), target_pixels_in.max_y() };

    const auto description = wse::oui::ProjectionMeshAdapter::createMesh(
        mesh, target_extent_in, source_extent_in );
    if ( !description.succeeded() )
    {
        return wse::oui::RendererResult< wse::oui::sMeshHandle >::failure( description.error() );
    }
    return p_renderer_inout->createMesh( description.value() );
}

//! Builds the screen-to-projection keystone warp: a 5x5 grid whose destination points
//! run through a four-point Homography that narrows the top edge.
wse::oui::RendererResult< wse::oui::sMeshHandle > createKeystoneMesh(
    wse::oui::Renderer* p_renderer_inout )
{
    const double screen_right = static_cast< double >( SCREEN_WIDTH - 1U );
    const double screen_bottom = static_cast< double >( SCREEN_HEIGHT - 1U );
    const double projection_right = static_cast< double >( PROJECTION_WIDTH - 1U );
    const double projection_bottom = static_cast< double >( PROJECTION_HEIGHT - 1U );
    const std::vector< wse::float64_xy > screen_corners = {
          { 0.0, 0.0 }
        , { screen_right, 0.0 }
        , { screen_right, screen_bottom }
        , { 0.0, screen_bottom }
    };
    const std::vector< wse::float64_xy > projection_corners = {
          { projection_right * 0.12, 0.0 }
        , { projection_right * 0.88, 0.0 }
        , { projection_right, projection_bottom }
        , { 0.0, projection_bottom }
    };
    const wse::Homography homography( screen_corners, projection_corners );

    constexpr std::size_t GRID = 5;
    wse::float64_mesh2d mesh( GRID, GRID );
    for ( std::size_t row = 0; row < GRID; ++row )
    {
        for ( std::size_t column = 0; column < GRID; ++column )
        {
            const wse::float64_xy source(
                screen_right * static_cast< double >( column ) / ( GRID - 1 ),
                screen_bottom * static_cast< double >( row ) / ( GRID - 1 ) );
            mesh[ row ][ column ].src = source;
            mesh[ row ][ column ].dst = homography.transform( source );
        }
    }
    const auto description = wse::oui::ProjectionMeshAdapter::createMesh(
        mesh, { PROJECTION_WIDTH, PROJECTION_HEIGHT }, { SCREEN_WIDTH, SCREEN_HEIGHT } );
    if ( !description.succeeded() )
    {
        return wse::oui::RendererResult< wse::oui::sMeshHandle >::failure( description.error() );
    }
    return p_renderer_inout->createMesh( description.value() );
}

//! Sample video 1: a diagonally scrolling color gradient.
void paintGradient( wse::img4c08_t* const p_image_inout, const int frame_in )
{
    wse::img4c08_t& image_inout = *p_image_inout;

    for ( std::uint32_t y = 0; y < SOURCE_HEIGHT; ++y )
    {
        for ( std::uint32_t x = 0; x < SOURCE_WIDTH; ++x )
        {
            auto& pixel = image_inout[ y ][ x ];
            pixel[ 0U ] = static_cast< std::uint8_t >( x + frame_in * 8 );
            pixel[ 1U ] = static_cast< std::uint8_t >( y + frame_in * 4 );
            pixel[ 2U ] = static_cast< std::uint8_t >( 255 - ( ( x + y ) / 2 ) );
            pixel[ 3U ] = 255U;
        }
    }
}

//! Sample video 2: a bright square bouncing on a dark background.
void paintBouncingSquare( wse::img4c08_t* const p_image_inout, const int frame_in )
{
    wse::img4c08_t& image_inout = *p_image_inout;

    const double phase = static_cast< double >( frame_in ) / FRAME_COUNT;
    const double triangle = 1.0 - std::fabs( 2.0 * phase - 1.0 );
    const std::uint32_t square = SOURCE_WIDTH / 4U;
    const std::uint32_t left =
        static_cast< std::uint32_t >( triangle * ( SOURCE_WIDTH - square ) );
    const std::uint32_t top =
        static_cast< std::uint32_t >( triangle * ( SOURCE_HEIGHT - square ) );
    for ( std::uint32_t y = 0; y < SOURCE_HEIGHT; ++y )
    {
        for ( std::uint32_t x = 0; x < SOURCE_WIDTH; ++x )
        {
            const bool inside = x >= left && x < left + square && y >= top && y < top + square;
            auto& pixel = image_inout[ y ][ x ];
            pixel[ 0U ] = inside ? 255U : 24U;
            pixel[ 1U ] = inside ? 160U : 24U;
            pixel[ 2U ] = inside ? 32U : 48U;
            pixel[ 3U ] = 255U;
        }
    }
}

//! Sample video 3: vertical color bars scrolling horizontally.
void paintColorBars( wse::img4c08_t* const p_image_inout, const int frame_in )
{
    wse::img4c08_t& image_inout = *p_image_inout;

    static const std::uint8_t BAR_COLORS[ 7 ][ 3 ] = {
        { 255U, 255U, 255U }, { 255U, 255U, 0U }, { 0U, 255U, 255U }, { 0U, 255U, 0U },
        { 255U, 0U, 255U }, { 255U, 0U, 0U }, { 0U, 0U, 255U }
    };
    const std::uint32_t bar_width = SOURCE_WIDTH / 7U + 1U;
    const std::uint32_t scroll = static_cast< std::uint32_t >( frame_in ) * 6U;
    for ( std::uint32_t y = 0; y < SOURCE_HEIGHT; ++y )
    {
        for ( std::uint32_t x = 0; x < SOURCE_WIDTH; ++x )
        {
            const std::uint32_t bar = ( ( x + scroll ) / bar_width ) % 7U;
            auto& pixel = image_inout[ y ][ x ];
            pixel[ 0U ] = BAR_COLORS[ bar ][ 0U ];
            pixel[ 1U ] = BAR_COLORS[ bar ][ 1U ];
            pixel[ 2U ] = BAR_COLORS[ bar ][ 2U ];
            pixel[ 3U ] = 255U;
        }
    }
}

} // namespace

int main()
{
    using namespace wse::oui;

    wse::registDefaultLog();

    // 1. Window creation. The default adapter is used first; WARP/llvmpipe is the fallback
    //    so the sample still runs on a machine without a usable GPU.
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
    if ( !capabilities.value().supports_window || !capabilities.value().supports_mesh_rendering )
    {
        wse::WLog() << "this environment cannot show a window; nothing to render";
        return 0;
    }
    wse::WLog() << "renderer ready, adapter=" << capabilities.value().adapter_name;

    sSurfaceDescription surface_description;
    surface_description.type = eSurfaceType::Window;
    surface_description.extent = { PROJECTION_WIDTH, PROJECTION_HEIGHT };
    surface_description.format = eRendererPixelFormat::Rgba8Unorm;
    surface_description.buffer_count = 2U;
    surface_description.vertical_sync = true;
    surface_description.visible = true;
    surface_description.title = "WSE windowed projection example";
    const auto surface = renderer.createSurface( surface_description );
    if ( !surface.succeeded() )
        return fail( "window surface creation", surface.error() );

    // 2. Texture creation: three sources, one screen, one projection target, and the
    //    two alpha maps used by the geometric stages.
    sTextureHandle sources[ 3 ];
    for ( auto& source : sources )
    {
        const auto created = renderer.createTexture(
            uploadableDescription( { SOURCE_WIDTH, SOURCE_HEIGHT },
                                   eRendererPixelFormat::Rgba8Unorm ) );
        if ( !created.succeeded() )
            return fail( "source texture creation", created.error() );
        source = created.value();
    }
    const auto screen = renderer.createTexture(
        renderTargetDescription( { SCREEN_WIDTH, SCREEN_HEIGHT } ) );
    if ( !screen.succeeded() )
        return fail( "screen texture creation", screen.error() );
    const auto projection = renderer.createTexture(
        renderTargetDescription( { PROJECTION_WIDTH, PROJECTION_HEIGHT } ) );
    if ( !projection.succeeded() )
        return fail( "projection texture creation", projection.error() );

    const auto source_alpha = renderer.createTexture(
        uploadableDescription( { SOURCE_WIDTH, SOURCE_HEIGHT },
                               eRendererPixelFormat::R8Unorm ) );
    const auto screen_alpha = renderer.createTexture(
        uploadableDescription( { SCREEN_WIDTH, SCREEN_HEIGHT },
                               eRendererPixelFormat::R8Unorm ) );
    if ( !source_alpha.succeeded() || !screen_alpha.succeeded() )
        return fail( "alpha texture creation",
                     source_alpha.succeeded() ? screen_alpha.error() : source_alpha.error() );
    // uploadTexture() takes the Core image directly; the image type names the pixel format.
    const auto source_alpha_upload = renderer.uploadTexture(
        source_alpha.value(), makeEdgeFadeAlpha( { SOURCE_WIDTH, SOURCE_HEIGHT } ) );
    if ( !source_alpha_upload.succeeded() ||
         !renderer.waitFence( source_alpha_upload.value(), FENCE_TIMEOUT_MS ).succeeded() )
        return fail( "source alpha map upload", source_alpha_upload.error() );
    const auto screen_alpha_upload = renderer.uploadTexture(
        screen_alpha.value(), makeEdgeFadeAlpha( { SCREEN_WIDTH, SCREEN_HEIGHT } ) );
    if ( !screen_alpha_upload.succeeded() ||
         !renderer.waitFence( screen_alpha_upload.value(), FENCE_TIMEOUT_MS ).succeeded() )
        return fail( "screen alpha map upload", screen_alpha_upload.error() );

    // 3. Source-to-screen geometry: the three sources land in separate screen regions.
    const wse::float64_rect regions[ 3 ] = {
          { 8.0, SCREEN_WIDTH / 2.0 - 8.0, 8.0, SCREEN_HEIGHT - 9.0 }
        , { SCREEN_WIDTH / 2.0 + 8.0, SCREEN_WIDTH - 9.0, 8.0, SCREEN_HEIGHT / 2.0 - 8.0 }
        , { SCREEN_WIDTH / 2.0 + 8.0, SCREEN_WIDTH - 9.0, SCREEN_HEIGHT / 2.0 + 8.0,
            SCREEN_HEIGHT - 9.0 }
    };
    sMeshHandle source_meshes[ 3 ];
    for ( int index = 0; index < 3; ++index )
    {
        const auto mesh = createRegionMesh( &renderer, { SOURCE_WIDTH, SOURCE_HEIGHT },
                                            regions[ index ], { SCREEN_WIDTH, SCREEN_HEIGHT } );
        if ( !mesh.succeeded() )
            return fail( "source mesh creation", mesh.error() );
        source_meshes[ index ] = mesh.value();
    }

    // 4. Screen-to-projection geometry: one keystone warp derived from a Homography.
    const auto keystone = createKeystoneMesh( &renderer );
    if ( !keystone.succeeded() )
        return fail( "keystone mesh creation", keystone.error() );

    // The window pass samples the projection texture through a full-screen quad.
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
        return fail( "window quad creation", quad.error() );

    // 5.-7. Render 30 frames: sources -> screen -> projection -> window.
    // One reused RGBA image is the scratch buffer each sample video paints into.
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

        // Generate this frame of each sample video and upload it into its source texture.
        for ( int source_index = 0; source_index < 3; ++source_index )
        {
            if ( source_index == 0 ) paintGradient( &source_image, frame_index );
            else if ( source_index == 1 ) paintBouncingSquare( &source_image, frame_index );
            else paintColorBars( &source_image, frame_index );
            const auto upload =
                renderer.uploadTexture( sources[ source_index ], source_image );
            if ( !upload.succeeded() ||
                 !renderer.waitFence( upload.value(), FENCE_TIMEOUT_MS ).succeeded() )
                return fail( "source upload", upload.error() );
        }

        // 5. Source -> screen: three warped, alpha-masked layers composited on one target.
        sProjectionPassDescription screen_pass;
        screen_pass.color_attachment = screen.value();
        screen_pass.clear_color = { 0.04F, 0.04F, 0.06F, 1.0F };
        for ( int source_index = 0; source_index < 3; ++source_index )
        {
            screen_pass.layers.emplace_back( sProjectionLayerDescription{
                  source_meshes[ source_index ]
                , sources[ source_index ]
                , source_alpha.value()
                , eTextureSamplingFilter::Linear
                , 1.0F
                , {}
            } );
        }
        screen_pass.final_state = eTextureState::ShaderResource;
        const auto screen_result = ProjectionPipeline::execute( &renderer, screen_pass );
        if ( !screen_result.succeeded() )
            return fail( "screen pass", screen_result.error() );

        // 6. Screen -> projection: one keystone-warped, alpha-masked layer.
        sProjectionPassDescription projection_pass;
        projection_pass.color_attachment = projection.value();
        projection_pass.clear_color = { 0.0F, 0.0F, 0.0F, 1.0F };
        projection_pass.layers.emplace_back( sProjectionLayerDescription{
              keystone.value()
            , screen.value()
            , screen_alpha.value()
            , eTextureSamplingFilter::Linear
            , 1.0F
            , {}
        } );
        projection_pass.final_state = eTextureState::ShaderResource;
        const auto projection_result = ProjectionPipeline::execute( &renderer, projection_pass );
        if ( !projection_result.succeeded() )
            return fail( "projection pass", projection_result.error() );

        // 7. Projection -> window: the current back buffer must end in the Present state.
        const auto backbuffer = renderer.getSurfaceTexture( surface.value() );
        if ( !backbuffer.succeeded() )
            return fail( "back-buffer lookup", backbuffer.error() );
        sRenderPassDescription window_pass;
        window_pass.color_attachment = backbuffer.value();
        window_pass.clear_color = { 0.0F, 0.0F, 0.0F, 1.0F };
        window_pass.draw_commands.push_back( sMeshDrawCommand{
              quad.value()
            , projection.value()
            , {}
            , eTextureSamplingFilter::Linear
            , eColorBlendMode::Replace
            , 1.0F
            , {}
        } );
        window_pass.final_state = eTextureState::Present;
        const auto window_result = renderer.executeRenderPass( window_pass );
        if ( !window_result.succeeded() )
            return fail( "window pass", window_result.error() );

        const auto present_result = renderer.presentSurface( surface.value() );
        if ( !present_result.succeeded() )
            return fail( "present", present_result.error() );
        if ( !renderer.waitFence( present_result.value(), FENCE_TIMEOUT_MS ).succeeded() )
            return fail( "present fence wait", present_result.error() );
        ++presented;
        std::this_thread::sleep_for( FRAME_INTERVAL );
    }

    wse::WLog() << "presented frames:" << presented << "of" << FRAME_COUNT;

    renderer.destroyMesh( quad.value() );
    renderer.destroyMesh( keystone.value() );
    for ( const auto& mesh : source_meshes ) renderer.destroyMesh( mesh );
    renderer.destroyTexture( screen_alpha.value() );
    renderer.destroyTexture( source_alpha.value() );
    renderer.destroyTexture( projection.value() );
    renderer.destroyTexture( screen.value() );
    for ( const auto& source : sources ) renderer.destroyTexture( source );
    renderer.destroySurface( surface.value() );
    renderer.shutdown();
    return 0;
}
