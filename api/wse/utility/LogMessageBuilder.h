// @file LogMessageBuilder.h
// @brief Bounded WSE-compatible value conversion and separator construction for asynchronous frontends.
#pragma once

#include <string_view>
#include <type_traits>
#include "AsyncLogRuntime.h"

namespace wse
{
    //! Not thread safe. Each ordinary append prepends the separator, including the first and empty text.
    class WSE_API LogMessageBuilder final
    {
    private:
        std::vector<sLogPart> m_parts;
        std::string m_separator;
        std::size_t m_byte_limit;
        std::size_t m_part_limit;
        std::size_t m_bytes;
        bool m_is_valid;
    public:
        explicit LogMessageBuilder( const eLogSeparate separate_in = eLogSeparate::SPACE,
            const std::size_t byte_limit_in = 16 * 1024, const std::size_t part_limit_in = 1024 );
        LogMessageBuilder& append( const std::string& text_in );
        LogMessageBuilder& append( const std::string_view text_in );
        LogMessageBuilder& append( const char* p_text_in );
        LogMessageBuilder& append( const std::wstring& text_in );
        LogMessageBuilder& append( const wchar_t* p_text_in );
        LogMessageBuilder& append( const bool value_in );
        LogMessageBuilder& append( const eLogControl control_in );
        LogMessageBuilder& appendEnum( const std::uint64_t bits_in, const std::size_t size_in );
        template<typename T, std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, int> = 0>
        LogMessageBuilder& append( const T value_in )
        {
            try { return this->append( std::to_string( value_in ) ); }
            catch( ... ) { this->m_is_valid = false; throw; }
        }
        template<typename T, std::enable_if_t<std::is_enum<T>::value, int> = 0>
        LogMessageBuilder& append( const T value_in )
        {
            return this->appendEnum( static_cast<std::uint64_t>( value_in ), sizeof( T ) );
        }
        template<typename T>
        LogMessageBuilder& operator<<( const T& value_in ) { return this->append( value_in ); }
        LogMessageBuilder& operator<<( const eLogControl control_in ) { return this->append( control_in ); }
        bool isValid() const noexcept;
        std::size_t byteCount() const noexcept;
        const std::vector<sLogPart>& parts() const noexcept;
    };

    //! One RAII record; the runtime must outlive this frontend. Destruction only enqueues owned values.
    class WSE_API AsyncLogLine final
    {
    private:
        AsyncLogRuntime* m_ptr_runtime;
        sAsyncLogRecord m_record;
        LogMessageBuilder m_builder;
        bool m_is_enabled;
    public:
        AsyncLogLine( AsyncLogRuntime* p_runtime_inout, const std::string& tag_in,
            const unsigned verbosity_in = 1, const std::string& file_in = {},
            const int line_in = 0, const LogLevel level_in = LogLevel::Info );
        ~AsyncLogLine() noexcept;
        AsyncLogLine( const AsyncLogLine& ) = delete;
        AsyncLogLine& operator=( const AsyncLogLine& ) = delete;
        template<typename T>
        AsyncLogLine& operator<<( const T& value_in )
        {
            if( this->m_is_enabled ) { this->m_builder.append( value_in ); }
            return *this;
        }
        AsyncLogLine& operator<<( const eLogControl control_in );
        AsyncLogLine& setLevel( const LogLevel level_in ) noexcept;
    };
}
