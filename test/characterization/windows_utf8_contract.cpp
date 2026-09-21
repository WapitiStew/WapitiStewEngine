// SPDX-License-Identifier: Apache-2.0
#include "../../platform/wse/win/utility/Utf8.h"
#include <iostream>

int main()
{
    const auto check = []( const bool condition_in )
    {
        if( !condition_in ) { throw std::runtime_error( "Windows UTF-8 conversion contract failed" ); }
    };
    check( wse::detail::toUtf8( L"" ).empty() );
    check( wse::detail::toUtf8( L"Camera 01" ) == "Camera 01" );
    check( wse::detail::toUtf8( L"\u65e5\u672c\u8a9e" ) == "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e" );
    check( wse::detail::toUtf8( L"\U0001f4f7" ) == "\xf0\x9f\x93\xb7" );
    check( wse::detail::toUtf8( std::wstring( L"a\0b", 3 ) ) == std::string( "a\0b", 3 ) );
    for( const wchar_t surrogate : { wchar_t(0xd800), wchar_t(0xdc00) } )
    {
        bool rejected = false;
        try { (void)wse::detail::toUtf8( std::wstring( 1, surrogate ) ); }
        catch( const std::range_error& ) { rejected = true; }
        check( rejected );
    }
    std::cout << "Windows UTF-8 contract passed\n";
}
