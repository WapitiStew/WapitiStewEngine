// SPDX-License-Identifier: Apache-2.0
#ifndef WSE_WIN_UTILITY_UTF8_H
#define WSE_WIN_UTILITY_UTF8_H

#include <windows.h>
#include <limits>
#include <stdexcept>
#include <string>

namespace wse { namespace detail {

// Preserve embedded NULs and reject malformed UTF-16, including lone surrogates.
inline std::string toUtf8( const std::wstring& text_in )
{
    if( text_in.empty() ) { return {}; }
    if( text_in.size() > static_cast<std::size_t>( (std::numeric_limits<int>::max)() ) )
    {
        throw std::length_error( "UTF-16 input exceeds the Windows conversion limit" );
    }
    const int length = static_cast<int>( text_in.size() );
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text_in.data(), length, nullptr, 0, nullptr, nullptr );
    if( required == 0 ) { throw std::range_error( "invalid UTF-16 input" ); }

    std::string result( static_cast<std::size_t>( required ), '\0' );
    if( WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, text_in.data(), length,
                           result.data(), required, nullptr, nullptr ) != required )
    {
        throw std::range_error( "UTF-16 conversion failed" );
    }
    return result;
}

} } // namespace wse::detail
#endif
