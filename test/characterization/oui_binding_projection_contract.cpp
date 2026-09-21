//*****************************************************************************************************************
//!
//! @file    oui_binding_projection_contract.cpp
//! @brief   \~japanese Handleを公開しないBinding Projection Facadeの契約を検証する.
//! @brief   \~english  Verifies the handle-free Projection binding facade contract.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-29, 2026   Create New.
//!
//! @details
//!   \~japanese
//!     renderProjection()はC#／Java／JavaScript／PythonのBindingが共通で叩く唯一のProjection
//!     入口である. 同じFixture( test/golden/binding_projection_full_frame.properties )を各言語の
//!     Testも読むため, ここが動くと全Bindingの期待Byteが同時にずれる. 本Testが固定するのは,
//!     指定した出力Extentとrow_pitchでPacked RGBA8が返ること, Nearest拡大の全Frame Byteが
//!     Golden Fixtureと1 Byteも違わないこと, 描いたAdapter名が必ず報告されること, そして
//!     不正な入力や矛盾したAdapter指定がValidation分類の構造化Errorとして返り, 黙って別の
//!     Adapterで描かれたりしないことである.
//!   \~english
//!     renderProjection() is the single Projection entry point every language binding calls, and the
//!     C#, Java, JavaScript, and Python tests read the same fixture
//!     ( test/golden/binding_projection_full_frame.properties ) that this test reads. A change here
//!     moves the expected bytes of every binding at once. The test pins that the result carries the
//!     requested output extent and row pitch as packed RGBA8, that the whole nearest-magnified frame
//!     matches the golden fixture byte for byte, that the adapter which actually drew the frame is
//!     always reported, and that bad input or a contradictory adapter request comes back as a
//!     structured Validation error instead of a quiet render on some other adapter.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#include <oui/binding/Projection.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
void fail( const std::string& message_in )
{
    std::cerr << message_in << '\n';
}

// A deliberately minimal .properties reader: key=value, '#' comments, and a tolerated trailing CR so
// the same fixture file works whether the checkout uses LF or CRLF. It has no escapes, no sections
// and no type conversion, because every language binding has to be able to read this same fixture
// with its own standard library. A duplicate key keeps the first occurrence, since emplace does not
// overwrite; the fixture is expected not to contain one.
std::unordered_map<std::string, std::string> loadProperties( const char* path_in )
{
    std::ifstream input( path_in );
    std::unordered_map<std::string, std::string> result;
    std::string line;
    while ( std::getline( input, line ) )
    {
        if ( !line.empty() && line.back() == '\r' ) line.pop_back();
        if ( line.empty() || line.front() == '#' ) continue;
        const std::size_t separator = line.find( '=' );
        if ( separator != std::string::npos )
            result.emplace( line.substr( 0U, separator ), line.substr( separator + 1U ) );
    }
    return result;
}

// The fixture stores pixels as hex text so that it is byte-exact and diffable, and so that no
// language binding has to agree on a binary container. An odd-length string returns an empty vector
// rather than throwing, so a corrupted fixture surfaces later as a size mismatch, not here.
std::vector<std::uint8_t> decodeHex( const std::string& value_in )
{
    std::vector<std::uint8_t> result;
    if ( value_in.size() % 2U != 0U ) return result;
    result.reserve( value_in.size() / 2U );
    for ( std::size_t offset = 0U; offset < value_in.size(); offset += 2U )
    {
        result.emplace_back( static_cast<std::uint8_t>(
            std::stoul( value_in.substr( offset, 2U ), nullptr, 16 ) ) );
    }
    return result;
}

// FNV-1a 64 with the standard offset basis and prime. It is not chosen for collision resistance but
// because it is four lines in every one of the binding languages, so each of them can quote the one
// hash value in the fixture instead of shipping its own copy of the expected pixels.
std::uint64_t fnv1a64( const std::vector<std::uint8_t>& bytes_in )
{
    std::uint64_t result = 14695981039346656037ULL;
    for ( const std::uint8_t value : bytes_in )
    {
        result ^= value;
        result *= 1099511628211ULL;
    }
    return result;
}
}

int main( const int count_in, const char* const* const arguments_in )
{
    if ( count_in < 3 )
    {
        fail( "Projection binding contract requires backend and fixture arguments." );
        return 1;
    }
    // Argument 1 is the backend to exercise and argument 2 is the shared golden fixture. Both are
    // supplied by the build system so the same source runs against D3D12 on Windows and Vulkan on
    // Linux without a platform #if here.
    const auto fixture = loadProperties( arguments_in[2] );

    // A missing fixture key is a broken test setup rather than a product failure, so it throws and
    // is reported separately from a render mismatch. Reading the fixture with a default value would
    // be worse: the test would then silently compare against zeros.
    const auto required = [&fixture]( const char* key_in ) -> const std::string&
    {
        const auto found = fixture.find( key_in );
        if ( found == fixture.end() ) throw std::runtime_error(
            std::string( "Missing Projection fixture property: " ) + key_in );
        return found->second;
    };

    // Only the two extents are parsed inside the try, because they are the only fixture values whose
    // absence would otherwise render a 0 x 0 target and fail with a confusing backend error.
    wse::oui::sProjectionRenderRequest request;
    try
    {
        request.output_width = static_cast<std::uint32_t>(
            std::stoul( required( "output_width" ) ) );
        request.output_height = static_cast<std::uint32_t>(
            std::stoul( required( "output_height" ) ) );
    }
    catch ( const std::exception& error )
    {
        fail( std::string( "Unable to load Projection fixture: " ) + error.what() );
        return 1;
    }
    // Opaque black clear, so any pixel that differs from the fixture came from the layer and not
    // from a background the fixture cannot describe. The software adapter is requested because the
    // golden bytes must be reproducible on a build agent with no GPU; that is also what makes the
    // comparison exact rather than tolerance-based.
    request.clear_color = { 0.0F, 0.0F, 0.0F, 1.0F };
    request.use_software_adapter = true;
    if ( count_in > 1 && std::string( arguments_in[ 1 ] ) == "vulkan" )
        request.backend = wse::oui::eRendererBackend::Vulkan12;
    else
        request.backend = wse::oui::eRendererBackend::Direct3D12;

    // One layer: a small source image on a full-target quad. Nearest sampling is what makes the
    // expected bytes exactly reproducible across D3D12 and Vulkan - a Linear filter would put the
    // two backends' interpolation rules into the golden bytes and the fixture could not be shared.
    // The four vertices span clip space corner to corner with UV 0..1, and the two triangles are
    // wound so the whole target is covered; that is why every output pixel is a magnified source
    // texel and the comparison can be a full-frame byte equality.
    wse::oui::sProjectionImageLayer layer;
    layer.width = static_cast<std::uint32_t>( std::stoul( required( "source_width" ) ) );
    layer.height = static_cast<std::uint32_t>( std::stoul( required( "source_height" ) ) );
    layer.sampling_filter = wse::oui::eTextureSamplingFilter::Nearest;
    layer.rgba = decodeHex( required( "source_rgba_hex" ) );
    layer.vertices = {
        { -1.0F,  1.0F, 0.0F, 0.0F },
        {  1.0F,  1.0F, 1.0F, 0.0F },
        { -1.0F, -1.0F, 0.0F, 1.0F },
        {  1.0F, -1.0F, 1.0F, 1.0F },
    };
    layer.indices = { 0U, 1U, 2U, 2U, 1U, 3U };
    request.layers.emplace_back( std::move( layer ) );

    // The whole GPU lifecycle - device, surface, texture upload, mesh, pass, fence, readback - is
    // meant to open and close inside this one call, with no handle reaching the caller. That is what
    // lets a managed language bind Projection at all.
    const auto output = wse::oui::renderProjection( request );
    if ( !output.succeeded() )
    {
        fail( "Projection binding contract failed: " + output.error().message() );
        return 1;
    }
    // The layout is checked before the pixels so that a mismatch reads as a layout defect rather
    // than as a wall of differing bytes. row_pitch must be exactly width * 4: the binding contract
    // is tightly packed RGBA8, and a backend that leaked its own padded readback pitch through here
    // would make every managed caller index into the wrong row.
    const std::vector<std::uint8_t> expected = decodeHex(
        required( "expected_rgba_hex" ) );
    const std::uint64_t expected_hash = std::stoull(
        required( "expected_fnv1a64" ), nullptr, 16 );
    if ( output.value().width != request.output_width
         || output.value().height != request.output_height
         || output.value().row_pitch != request.output_width * 4U
         || output.value().data.size() != expected.size() )
    {
        fail( "Projection binding frame layout is invalid." );
        return 1;
    }
    // Every byte of the frame is compared, not a sample or a checksum alone, because the bindings in
    // the other languages compare against this same fixture and a partial check here would let them
    // drift. The hash is compared as well; it cannot fail once the byte comparison has passed, so
    // what it actually guards is the fixture itself - it catches a hand-edited expected_rgba_hex
    // whose expected_fnv1a64 was not regenerated, before the other languages trust that hash.
    const auto& bytes = output.value().data.bytes();
    if ( bytes != expected || fnv1a64( bytes ) != expected_hash )
    {
        fail( "Projection binding full-frame bytes or FNV-1a hash differ from the fixture." );
        return 1;
    }

    // Which adapter drew the frame is reported whether or not one was named on the request. Naming
    // none is the common case, and it is the case where the report is the only way to find out.
    if ( output.value().adapter_name.empty() )
    {
        fail( "Projection did not report the adapter that drew the frame." );
        return 1;
    }

    // The binding facade must supply the explicit render extent required by scale 2.
    // A uniform opaque source has a backend-independent full-frame result after both passes.
    {
        auto supersampled = request;
        supersampled.supersample_scale = 2U;
        auto& source = supersampled.layers[0].rgba;
        for ( std::size_t offset = 0; offset < source.size(); offset += 4U )
        {
            source[offset] = 64U; source[offset + 1U] = 128U;
            source[offset + 2U] = 192U; source[offset + 3U] = 255U;
        }
        const auto frame = wse::oui::renderProjection( supersampled );
        if ( !frame.succeeded() )
        {
            fail( "Binding scale 2 failed: " + frame.error().message() );
            return 1;
        }
        const auto& data = frame.value().data.bytes();
        if ( data.size() != expected.size() || frame.value().width != request.output_width ||
             frame.value().height != request.output_height ||
             frame.value().row_pitch != request.output_width * 4U ) return 1;
        const std::uint8_t pixel[] = { 64U, 128U, 192U, 255U };
        for ( std::size_t index = 0; index < data.size(); ++index )
            if ( data[index] != pixel[index % 4U] )
            {
                fail( "Binding scale 2 changed a uniform source pixel." );
                return 1;
            }
    }

    // A named adapter that no machine has is a failure rather than a quiet render on another one.
    // Note that `request` still carries use_software_adapter, so initialize() refuses the pair
    // before it ever tries to match the name. As written, this establishes that the request is
    // refused, not yet that an unmatchable name on its own is what refuses it.
    {
        auto named = request;
        named.adapter_name = "no-such-adapter-anywhere";
        const auto refused = wse::oui::renderProjection( named );
        if ( refused.succeeded() )
        {
            fail( "Projection accepted an adapter name that matches nothing." );
            return 1;
        }
    }

    // Naming an adapter and asking for the software adapter are two different requests, so asking
    // for both is refused before either backend is reached.
    {
        auto contradictory = request;
        contradictory.use_software_adapter = true;
        contradictory.adapter_name = output.value().adapter_name;
        const auto refused = wse::oui::renderProjection( contradictory );
        if ( refused.succeeded()
             || refused.error().category() != wse::oui::eRendererErrorCategory::Validation )
        {
            fail( "Projection accepted both an adapter name and the software adapter." );
            return 1;
        }
    }

    request.layers[0].rgba.pop_back();
    const auto invalid = wse::oui::renderProjection( request );
    if ( invalid.succeeded()
         || invalid.error().category() != wse::oui::eRendererErrorCategory::Validation )
    {
        fail( "Projection binding invalid-input error is not structured." );
        return 1;
    }
    return 0;
}
