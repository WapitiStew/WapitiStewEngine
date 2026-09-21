// Real offscreen backends: foreign/stale identities must not alias live resources.
#include <oui/renderer/Renderer.h>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
using namespace wse::oui;
constexpr std::uint32_t timeout_ms = 30000U;
int failures = 0;
void expect( bool condition_in, const char* message_in )
{
    if( !condition_in ) { ++failures; std::cerr << "FAILED: " << message_in << '\n'; }
}
template< class Result > void require( const Result& result_in )
{
    if( !result_in.succeeded() ) throw std::runtime_error( result_in.error().message() );
}
template< class Result > void missing( const Result& result_in, const char* message_in )
{
    expect( !result_in.succeeded() && result_in.error().code() == eRendererErrorCode::ResourceNotFound, message_in );
}
sRendererConfiguration configuration()
{
    sRendererConfiguration value;
#ifdef _WIN32
    value.backend = eRendererBackend::Direct3D12;
#else
    value.backend = eRendererBackend::Vulkan12;
#endif
    return value;
}
sMeshDescription meshDescription()
{
    sMeshDescription value;
    value.vertices = { {-1,1,0,0}, {1,1,1,0}, {1,-1,1,1}, {-1,-1,0,1} };
    value.indices = {0,1,2,0,2,3};
    return value;
}
sRendererFrame pixels()
{
    sRendererFrame value;
    value.description.extent = {1,1};
    value.description.format = eRendererPixelFormat::Rgba8Unorm;
    value.data = {19,71,137,255};
    return value;
}
struct Resources
{
    sTextureHandle texture;
    sMeshHandle mesh;
    sSurfaceHandle surface;
    sTextureHandle attachment;
    sFenceHandle fence;
};
Resources createResources( Renderer& renderer_inout )
{
    sTextureDescription description;
    description.extent = {1,1};
    description.format = eRendererPixelFormat::Rgba8Unorm;
    description.usage = eTextureUsage::Sampled | eTextureUsage::TransferSource | eTextureUsage::TransferDestination;
    description.initial_state = eTextureState::CopyDestination;
    const auto texture = renderer_inout.createTexture( description ); require( texture );
    const auto upload = renderer_inout.uploadTexture( texture.value(), pixels() ); require( upload );
    require( renderer_inout.waitFence( upload.value(), timeout_ms ) );
    const auto mesh = renderer_inout.createMesh( meshDescription() ); require( mesh );
    sSurfaceDescription surface_description;
    surface_description.extent = {4,4};
    surface_description.visible = false;
    const auto surface = renderer_inout.createSurface( surface_description ); require( surface );
    const auto attachment = renderer_inout.getSurfaceTexture( surface.value() ); require( attachment );
    return {texture.value(),mesh.value(),surface.value(),attachment.value(),upload.value()};
}
void live( Renderer& renderer_inout, const Resources& resources_in )
{
    const auto frame = renderer_inout.readTexture( resources_in.texture, timeout_ms );
    expect( frame.succeeded() && frame.value().data == pixels().data, "Live texture pixels survive rejected foreign operations" );
    expect( renderer_inout.updateMesh( resources_in.mesh, meshDescription() ).succeeded(), "Live mesh survives" );
    const auto state = renderer_inout.getSurfaceState( resources_in.surface );
    expect( state.succeeded() && state.value().extent.width == 4U && state.value().extent.height == 4U,
        "Live surface extent survives" );
    const auto attachment = renderer_inout.getSurfaceTexture( resources_in.surface );
    expect( attachment.succeeded() && attachment.value().value == resources_in.attachment.value
        && attachment.value().generation == resources_in.attachment.generation, "Surface attachment is unchanged" );
    expect( renderer_inout.waitFence( resources_in.fence, timeout_ms ).succeeded(), "Live fence remains waitable" );
}
void reject( Renderer& renderer_inout, const Resources& wrong_in, const Resources& live_in )
{
    missing( renderer_inout.readTexture( wrong_in.texture, timeout_ms ), "Foreign/stale texture read" );
    missing( renderer_inout.uploadTexture( wrong_in.texture, pixels() ), "Foreign/stale upload" );
    missing( renderer_inout.updateMesh( wrong_in.mesh, meshDescription() ), "Foreign/stale mesh update" );
    missing( renderer_inout.getSurfaceTexture( wrong_in.surface ), "Foreign/stale surface texture" );
    missing( renderer_inout.getSurfaceState( wrong_in.surface ), "Foreign/stale surface state" );
    missing( renderer_inout.resizeSurface( wrong_in.surface, {8,8} ), "Foreign/stale surface resize" );
    missing( renderer_inout.setSurfaceWindowMode( wrong_in.surface, {} ), "Foreign/stale window mode" );
    missing( renderer_inout.pollSurfaceEvents( wrong_in.surface ), "Foreign/stale surface poll" );
    missing( renderer_inout.processSurfaceEvents( wrong_in.surface ), "Foreign/stale surface events" );
    missing( renderer_inout.presentSurface( wrong_in.surface ), "Foreign/stale present" );
    missing( renderer_inout.waitFence( wrong_in.fence, timeout_ms ), "Foreign/stale fence wait" );
    sRenderPassDescription pass;
    pass.color_attachment = wrong_in.attachment;
    pass.final_state = eTextureState::CopySource;
    missing( renderer_inout.executeRenderPass( pass ), "Foreign/stale color attachment" );
    pass.color_attachment = live_in.attachment;
    sMeshDrawCommand draw;
    draw.mesh = wrong_in.mesh;
    draw.source_texture = live_in.texture;
    pass.draw_commands = {draw};
    missing( renderer_inout.executeRenderPass( pass ), "Foreign/stale draw mesh" );
    pass.draw_commands.front().mesh = live_in.mesh;
    pass.draw_commands.front().source_texture = wrong_in.texture;
    missing( renderer_inout.executeRenderPass( pass ), "Foreign/stale sampled texture" );
    pass.draw_commands.front().source_texture = live_in.texture;
    pass.draw_commands.front().alpha_texture = wrong_in.texture;
    missing( renderer_inout.executeRenderPass( pass ), "Foreign/stale alpha texture" );
    missing( renderer_inout.destroyTexture( wrong_in.texture ), "Foreign/stale texture destroy" );
    missing( renderer_inout.destroyMesh( wrong_in.mesh ), "Foreign/stale mesh destroy" );
    missing( renderer_inout.destroySurface( wrong_in.surface ), "Foreign/stale surface destroy" );
    live( renderer_inout, live_in );
}
void configurationContract()
{
    Renderer renderer;
    const auto original = configuration();
    require( renderer.initialize( original ) );
    const auto resources = createResources( renderer );
    require( renderer.initialize( original ) );
    auto changed = original;
    // A valid, different requested name still selects this adapter; it must not be ignored.
    changed.adapter_name = renderer.getCapabilities().value().adapter_name;
    expect( !changed.adapter_name.empty(), "Reported adapter name is nonempty" );
    const auto change = renderer.initialize( changed );
    expect( !change.succeeded() && change.error().code() == eRendererErrorCode::AlreadyInitialized,
        "Changing only adapter_name is AlreadyInitialized" );
    live( renderer, resources );
    for( int field = 0; field < 4; ++field )
    {
        auto different = original;
        if( field == 0 ) different.backend = eRendererBackend::Automatic;
        if( field == 1 ) different.use_software_adapter = true;
        if( field == 2 ) different.prefer_display_adapter = true;
        if( field == 3 ) different.enable_validation = true;
        const auto result = renderer.initialize( different );
        expect( !result.succeeded() && result.error().code() == eRendererErrorCode::AlreadyInitialized,
            "Changed valid configuration preserves the initialized backend" );
    }
    live( renderer, resources );
    renderer.shutdown();
    require( renderer.initialize( changed ) );
    require( renderer.initialize( changed ) );
    const auto named_resources = createResources( renderer );
    const auto clear_name = renderer.initialize( original );
    expect( !clear_name.succeeded() && clear_name.error().code() == eRendererErrorCode::AlreadyInitialized,
        "Clearing only adapter_name is AlreadyInitialized" );
    auto invalid = changed;
    invalid.use_software_adapter = true;
    const auto refused = renderer.initialize( invalid );
    expect( !refused.succeeded() && refused.error().code() == eRendererErrorCode::InvalidArgument,
        "Invalid adapter/software combination keeps argument-validation precedence" );
    live( renderer, named_resources );
}
}
int main()
{
    try
    {
        configurationContract();
        Renderer first, second;
        require( first.initialize( configuration() ) );
        require( second.initialize( configuration() ) );
        const auto a = createResources( first ), b = createResources( second );
        expect( a.texture.value == b.texture.value && a.fence.value == b.fence.value,
            "Equal local counters exercise a real collision candidate" );
        expect( a.texture.generation != b.texture.generation, "Renderer lifetimes have distinct generations" );
        reject( second, a, b );
        live( first, a );
        first.shutdown();
        require( first.initialize( configuration() ) );
        const auto next = createResources( first );
        expect( a.texture.generation != next.texture.generation, "Reinitialization creates a fresh generation" );
        reject( first, a, next );
        Renderer moved( std::move( first ) );
        expect( !first.isInitialized(), "Move construction leaves the source uninitialized" );
        live( moved, next );
        require( first.initialize( configuration() ) );
        const auto fresh = createResources( first );
        reject( first, next, fresh );
        second = std::move( moved );
        expect( !moved.isInitialized(), "Move assignment leaves the source uninitialized" );
        reject( second, b, next );
        live( second, next );
        second = std::move( second );
        live( second, next );
    }
    catch( const std::exception& error ) { std::cerr << error.what() << '\n'; return 1; }
    if( failures ) return 1;
    std::cout << "Renderer configuration and resource identity contract passed\n";
    return 0;
}
