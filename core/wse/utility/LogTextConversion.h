// @file LogTextConversion.h
// @brief Shared platform conversion for synchronous and asynchronous log values.
// Copyright (C) 2025-2026 WapitiStew. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <limits>
#include <stdexcept>
#include <string>
#if defined(_WIN32)
#include <windows.h>
#else
#include <iconv.h>
#endif

namespace wse
{
    //! Convert the existing WSE wide-string representation without borrowing the caller's buffer.
    inline std::string wstring2UTF8( const std::wstring& text_in, const bool legacy_termination_in = false )
    {
        if( text_in.empty() ) { return {}; }
#if defined(_WIN32)
        // Legacy synchronous strings keep their original terminator behavior; typed text retains embedded NUL.
        if( text_in.size() > static_cast<std::size_t>( ( std::numeric_limits<int>::max )() ) )
        { throw std::length_error( "Wide log value is too large" ); }
        const int length = legacy_termination_in ? -1 : static_cast<int>( text_in.size() );
        const int size = WideCharToMultiByte( CP_UTF8, 0, text_in.data(), length, nullptr, 0, nullptr, nullptr );
        if( size <= 0 ) { return {}; }
        std::string result( static_cast<std::size_t>( size ), '\0' );
        if( WideCharToMultiByte( CP_UTF8, 0, text_in.data(), length, result.data(), size, nullptr, nullptr ) <= 0 )
        { return {}; }
        if( legacy_termination_in ) { result.resize( static_cast<std::size_t>( size ) - 1 ); }
        return result;
#else
        (void)legacy_termination_in;
        if( text_in.size() > std::numeric_limits<std::size_t>::max() / sizeof( wchar_t ) / 4 )
        { throw std::length_error( "Wide log value is too large" ); }
        std::size_t input_bytes = text_in.size() * sizeof( wchar_t );
        std::string result( input_bytes * 4, '\0' );
        auto converter = iconv_open( "UTF-8", "WCHAR_T" );
        if( converter == reinterpret_cast<iconv_t>( -1 ) ) { return {}; }
        auto* input = const_cast<char*>( reinterpret_cast<const char*>( text_in.data() ) );
        auto* output = result.data();
        std::size_t output_bytes = result.size();
        const auto converted = iconv( converter, &input, &input_bytes, &output, &output_bytes );
        iconv_close( converter );
        if( converted == static_cast<std::size_t>( -1 ) ) { return {}; }
        result.resize( result.size() - output_bytes );
        return result;
#endif
    }
}
