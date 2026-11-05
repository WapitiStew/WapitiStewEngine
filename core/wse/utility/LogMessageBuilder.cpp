// @file LogMessageBuilder.cpp
// @brief WSE value/separator compatibility with bounded text and explicit newline parts.
#include "../../../api/wse/utility/LogMessageBuilder.h"
#include "LogTextConversion.h"
#include <limits>
#include <stdexcept>

namespace wse
{
    LogMessageBuilder::LogMessageBuilder( const eLogSeparate separate_in,
        const std::size_t byte_limit_in, const std::size_t part_limit_in )
        : m_parts(), m_separator( getLogSeparator( separate_in ) ), m_byte_limit( byte_limit_in ),
          m_part_limit( part_limit_in ), m_bytes( 0 ),
          m_is_valid( !this->m_separator.empty() && byte_limit_in > 0 && part_limit_in > 0 ) {}

    LogMessageBuilder& LogMessageBuilder::append( const std::string_view text_in )
    {
        if( !this->m_is_valid ) { return *this; }
        // Check before making a copy; a rejected field never creates an unbounded temporary.
        if( text_in.size() > this->m_byte_limit - this->m_bytes ||
            this->m_separator.size() > this->m_byte_limit - this->m_bytes - text_in.size() )
        {
            this->m_is_valid = false;
            return *this;
        }
        try
        {
            // Copy the already bounded view before mutating a possibly aliased parts().back().text.
            const std::string text( text_in );
            if( this->m_parts.empty() || this->m_parts.back().kind != eLogPartKind::Text )
            {
                if( this->m_parts.size() >= this->m_part_limit )
                {
                    this->m_is_valid = false;
                    return *this;
                }
                this->m_parts.push_back( { eLogPartKind::Text, {} } );
            }
            this->m_parts.back().text.append( this->m_separator );
            this->m_parts.back().text.append( text );
            this->m_bytes += this->m_separator.size() + text.size();
        }
        catch( ... ) { this->m_is_valid = false; throw; }
        return *this;
    }
    LogMessageBuilder& LogMessageBuilder::append( const std::string& text_in )
    { return this->append( std::string_view( text_in ) ); }
    LogMessageBuilder& LogMessageBuilder::append( const char* p_text_in )
    {
        if( !p_text_in ) { this->m_is_valid = false; return *this; }
        const auto available = this->m_byte_limit - this->m_bytes;
        std::size_t length = 0;
        while( length <= available && p_text_in[length] != '\0' ) { ++length; }
        if( length > available ) { this->m_is_valid = false; return *this; }
        return this->append( std::string_view( p_text_in, length ) );
    }
    LogMessageBuilder& LogMessageBuilder::append( const std::wstring& text_in )
    {
        if( text_in.size() > this->m_byte_limit - this->m_bytes )
        { this->m_is_valid = false; return *this; }
        try { return this->append( wstring2UTF8( text_in ) ); }
        catch( ... ) { this->m_is_valid = false; throw; }
    }
    LogMessageBuilder& LogMessageBuilder::append( const wchar_t* p_text_in )
    {
        if( !p_text_in ) { this->m_is_valid = false; return *this; }
        const auto available = this->m_byte_limit - this->m_bytes;
        std::size_t length = 0;
        while( length <= available && p_text_in[length] != L'\0' ) { ++length; }
        if( length > available ) { this->m_is_valid = false; return *this; }
        try { return this->append( std::wstring( p_text_in, length ) ); }
        catch( ... ) { this->m_is_valid = false; throw; }
    }
    LogMessageBuilder& LogMessageBuilder::append( const bool value_in )
    { return this->append( value_in ? "true" : "false" ); }
    LogMessageBuilder& LogMessageBuilder::appendEnum( const std::uint64_t bits_in, const std::size_t size_in )
    {
        try
        {
            switch( size_in )
            {
                case 1: return this->append( toHex( static_cast<std::uint8_t>( bits_in ) ) );
                case 2: return this->append( toHex( static_cast<std::uint16_t>( bits_in ) ) );
                case 4: return this->append( toHex( static_cast<std::uint32_t>( bits_in ) ) );
                default: return this->append( toHex( bits_in ) );
            }
        }
        catch( ... ) { this->m_is_valid = false; throw; }
    }
    LogMessageBuilder& LogMessageBuilder::append( const eLogControl control_in )
    {
        const auto bytes = END_LINE().size();
        if( !this->m_is_valid || control_in != eLogControl::NewLine ||
            bytes > this->m_byte_limit - this->m_bytes || this->m_parts.size() >= this->m_part_limit )
        { this->m_is_valid = false; return *this; }
        try { this->m_parts.push_back( { eLogPartKind::NewLine, {} } ); }
        catch( ... ) { this->m_is_valid = false; throw; }
        this->m_bytes += bytes;
        return *this;
    }
    bool LogMessageBuilder::isValid() const noexcept { return this->m_is_valid; }
    std::size_t LogMessageBuilder::byteCount() const noexcept { return this->m_bytes; }
    const std::vector<sLogPart>& LogMessageBuilder::parts() const noexcept { return this->m_parts; }

    static sAsyncLogSettings lineSettings( AsyncLogRuntime* p_runtime_inout )
    {
        if( !p_runtime_inout ) { throw std::invalid_argument( "AsyncLogLine requires a runtime" ); }
        return p_runtime_inout->getSettings();
    }
    // The settings copy happens before constructing a bounded builder; later runtime filtering remains authoritative.
    AsyncLogLine::AsyncLogLine( AsyncLogRuntime* p_runtime_inout, const std::string& tag_in,
        const unsigned verbosity_in, const std::string& file_in, const int line_in, const LogLevel level_in )
        : m_ptr_runtime( p_runtime_inout ), m_record(), m_builder(), m_is_enabled( false )
    {
        const auto settings = lineSettings( p_runtime_inout );
        this->m_builder = LogMessageBuilder( settings.separate, settings.record_byte_limit, settings.record_part_limit );
        this->m_record.tag = tag_in;
        this->m_record.source = tag_in;
        this->m_record.file = file_in;
        this->m_record.line = line_in;
        this->m_record.verbosity = verbosity_in;
        this->m_record.level = level_in;
        this->m_is_enabled = p_runtime_inout->isEnabled( tag_in, verbosity_in, LogLevel::Error );
    }
    AsyncLogLine::~AsyncLogLine() noexcept
    {
        if( !this->m_is_enabled ) { return; }
        try
        {
            this->m_record.parts = this->m_builder.parts();
            if( !this->m_builder.isValid() )
            {
                // A deliberately invalid part is rejected and counted by the runtime; never emit a truncated message.
                this->m_record.parts.push_back( { static_cast<eLogPartKind>( -1 ), {} } );
            }
            this->m_ptr_runtime->tryAppend( this->m_record );
        }
        catch( ... ) {}
    }
    AsyncLogLine& AsyncLogLine::operator<<( const eLogControl control_in )
    { if( this->m_is_enabled ) { this->m_builder.append( control_in ); } return *this; }
    AsyncLogLine& AsyncLogLine::setLevel( const LogLevel level_in ) noexcept
    { this->m_record.level = level_in; return *this; }
}
