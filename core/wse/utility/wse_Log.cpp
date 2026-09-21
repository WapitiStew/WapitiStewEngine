//*****************************************************************************************************************
//! 
//! @file    wse_Log.cpp
//! @brief   \~japanese Tag別のLog設定とConsole／Fileへの出力を行うLoggerの実装.
//! @brief   \~english  Implements the logger with per-tag configuration and console/file output.
//! @author  WapitiStew
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   2021/05/20   Create New WapitiStew
//!   Aug-05, 2026   Add thread-safe structured log dispatch.
//!   Aug-11, 2026   Separate registered tags from structured feature sources.
//!
//!
//! @details  
//!
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // 
#endif
#include "../TemplateExport.h"
#include <utility>
#include "../../../api/wse/utility/wse_Log.h"
#include "../../../api/wse/utility/wse_StringTool.h"
#include "../../../platform/wse/wse_platform.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// C/C++
#include <time.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <ctime>
#include <mutex>
#include <memory>


#if defined(_WIN32) || defined(_WIN64)
#pragma warning(push)
    #pragma warning(disable: 4464)  // '_WIN32_WINNT_WIN10_TH2' は、'#if/#elif' を '0' に置換するプリプロセッサ マクロとして定義されていません。
    #pragma warning(disable: 4668)  // ../
    #pragma warning(disable: 4820)  // ../
    #pragma warning(disable: 5039)  // ../
    #pragma warning(disable: 5204)  // ../
    #include <windows.h>
#pragma warning(pop)

#else
    #include <iconv.h>
    #include <errno.h>
    #include <cstring>
    #include <sys/time.h>
#endif




#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4702)  // ../
#endif
namespace wse
{
    const char WSE_TAG[] = "WSE";
    const char WSE_DEV_TAG[] = "DEV";
    template class WSE_INSTANTIATION_API Logger<WSE_TAG>;
    template class WSE_INSTANTIATION_API Logger<WSE_DEV_TAG>;



    static std::string wstring2UTF8(const std::wstring& wstr_in) 
    {
        if ( wstr_in.empty() ){  return std::string(); }

    #if defined(_WIN32) || defined(_WIN64)
        // Windows: WideCharToMultiByte を使う
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr_in.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size_needed <= 0) { return ""; }

        std::string str( static_cast< uint64_t >( size_needed ) - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr_in.c_str(), -1, &str[0], size_needed, nullptr, nullptr);
        return str;

    #else
        // POSIX (Linux, macOS, Android, ChromeOS, iOS): iconv を使う
        iconv_t conv = iconv_open("UTF-8", "WCHAR_T");
        if (conv == (iconv_t)-1) { return ""; }

        size_t in_size = wstr_in.size() * sizeof(wchar_t);
        size_t out_size = in_size * 4; // UTF-8 文字は最大4バイト
        char* out_buffer = new char[out_size];
        char* out_ptr = out_buffer;
        const char* in_ptr = reinterpret_cast<const char*>(wstr_in.c_str());

        size_t in_bytes_left = in_size;
        size_t out_bytes_left = out_size;

        if (iconv(conv, const_cast<char**>(&in_ptr), &in_bytes_left, &out_ptr, &out_bytes_left) == (size_t)-1)
        {
            delete[] out_buffer;
            iconv_close(conv);
            return "";
        }

        std::string result(out_buffer, out_ptr - out_buffer);
        delete[] out_buffer;
        iconv_close(conv);
        return result;
    #endif
    }


    struct sLogProfile
    {
        eLogMode     mode        ;
        eLogSeparate separate   ;
        eLogPrefix   prefix      ;
        eLogSuffix   suffix      ;
        std::string       fullpath;

        //! @brief Construct all members with explicit defaults.
        sLogProfile(
              eLogMode mode_in = eLogMode::NONE
            , eLogSeparate separate_in = eLogSeparate::SPACE
            , eLogPrefix prefix_in = eLogPrefix::NONE
            , eLogSuffix suffix_in = eLogSuffix::NONE
            , const std::string& fullpath_in = ""
        )
            : mode     ( mode_in )
            , separate ( separate_in )
            , prefix   ( prefix_in )
            , suffix   ( suffix_in )
            , fullpath ( fullpath_in )
        {
        }
    };
    static std::unordered_map< std::string, sLogProfile > g_map_log_profile;
    static std::unordered_map< LogSinkHandle, std::shared_ptr<LogSink> > g_log_sinks;
    static constexpr uint64_t PROFIZE_STEP_SIZE = 255;    
    static LogSinkHandle g_next_sink_handle = 1;
    static LogLevel g_minimum_log_level = LogLevel::Info;
    bool g_can_rehash = false;
    static void updateMapSize( void )
    {
        // 空なら新規生成.
        if ( g_map_log_profile.empty() )
        {
            g_map_log_profile.reserve( PROFIZE_STEP_SIZE );
            g_can_rehash = true;
        }
        // 容量を超えたら確保しなおす.
        else if ( g_map_log_profile.size() % PROFIZE_STEP_SIZE == 0 )
        { 
            const size_t coeff =  g_map_log_profile.size() / PROFIZE_STEP_SIZE;
            g_map_log_profile.reserve( g_map_log_profile.size() + coeff * PROFIZE_STEP_SIZE );
            g_can_rehash = true;
        }
        else{}

        if( g_map_log_profile.bucket_count() < static_cast< size_t >( static_cast< double >( g_map_log_profile.size() ) * 1.2 ) )
        {
            if( g_can_rehash )
            {
                g_map_log_profile.rehash( ( g_map_log_profile.size() % PROFIZE_STEP_SIZE ) * PROFIZE_STEP_SIZE );
            }
        }
    
    }

    std::mutex g_state_mutex;
    std::mutex g_output_mutex;
    thread_local bool g_is_dispatching_sink = false;


    // 日付を yy/mm/dd 形式で取得
    static std::string getLocalDate() 
    {        
        std::time_t now = std::time(nullptr);
        std::tm local_tm;

    #if defined(_WIN32) || defined(_WIN64)
        localtime_s(&local_tm, &now);
    #else
        localtime_r(&now, &local_tm);
    #endif

        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << (local_tm.tm_year % 100) << "/"
            << std::setw(2) << std::setfill('0') << (local_tm.tm_mon + 1)    << "/"
            << std::setw(2) << std::setfill('0') <<  local_tm.tm_mday;

        return oss.str();
    }

    // 時間を hh:mm:ss.sss 形式で取得
    static std::string getLocalTime() 
    {
        std::time_t now = std::time(nullptr);
        std::tm local_tm;

        long milliseconds;
    #if defined(_WIN32) || defined(_WIN64)
        localtime_s(&local_tm, &now);
        SYSTEMTIME st;
        GetLocalTime(&st);
        milliseconds = st.wMilliseconds;
    #else
        localtime_r(&now, &local_tm);
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        milliseconds = tv.tv_usec / 1000; // マイクロ秒をミリ秒に変換
    #endif

        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << local_tm.tm_hour << ":"
            << std::setw(2) << std::setfill('0') << local_tm.tm_min << ":"
            << std::setw(2) << std::setfill('0') << local_tm.tm_sec << "."
            << std::setw(3) << std::setfill('0') << milliseconds;

        return oss.str();
    }

    static std::string getLogPrefix( const eLogPrefix pre_in )
    {
        switch( pre_in )
        {
            case eLogPrefix::TIME: return "[ " + getLocalTime() + " ]";
            case eLogPrefix::DATE: return "[ " + getLocalDate() + " ]";
            case eLogPrefix::FULL: return "[ " + getLocalDate() + "-" + getLocalTime() + " ]";
            case eLogPrefix::NONE: return "";
            default : return "";
        }
        // ここには来ないはず.
        return "";
    }

    //!
    //! @brief      パスからファイル名の抽出
    //! @param[in]  path_in パス月フルパス（相対/絶対パスOK）
    //! @return     ファイル名のみ
    //!
    //! __FILE__マクロがフルパス仕様であり、余計な情報かつセキュリティ的にもよくないためファイル名だけにする
    //! 戻り値がconstexprなのでコンパイル時に決定する（はず）→ const char* がコンパイル時に計算されるだけ
    //! ファイルフルパスはバイナリに含まれなくなる（はず）→ const char* がコンパイル時に計算されるだけ
    //!
    static const std::string extractFileName(const char* path_in)
    {
        const char* file = path_in;
        while (*path_in)
        {
            if ((*path_in == '/') || (*path_in == '\\'))
            {
                file = path_in + 1;
            }
            path_in++;
        }
        return file;
    }
    
    //!
    //! @brief      LINEタグを取得する
    //! @param[in]  line_in    行番号
    //! @return     LINEタグ
    //!
    static const std::string getLineTag( const int line_in )
    {
        char buffer[100];
        // %-10d: 幅6で左寄せ、空き部分はスペースで埋める
        std::snprintf(buffer, sizeof(buffer), "%-6d", line_in);
        std::string padded(buffer);
        return padded;
    }

    static std::string getTimestampUtc( void )
    {
        const auto now = std::chrono::system_clock::now();
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch() ) % 1000;
        const std::time_t time = std::chrono::system_clock::to_time_t( now );

        std::tm utc{};
    #if defined(_WIN32) || defined(_WIN64)
        gmtime_s( &utc, &time );
    #else
        gmtime_r( &time, &utc );
    #endif

        std::ostringstream stream;
        stream << std::put_time( &utc, "%Y-%m-%dT%H:%M:%S" )
               << '.' << std::setw( 3 ) << std::setfill( '0' ) << milliseconds.count()
               << 'Z';
        return stream.str();
    }

    static const char* getStructuredLevelName( const LogLevel level_in )
    {
        switch( level_in )
        {
            case LogLevel::Trace   : return "TRACE";
            case LogLevel::Debug   : return "DEBUG";
            case LogLevel::Info    : return "INFO";
            case LogLevel::Warning : return "WARN";
            case LogLevel::Error   : return "ERROR";
            case LogLevel::Off     : return "OFF";
            default                : return "UNKNOWN";
        }
    }

    static bool readLogProfile( sLogProfile* const p_profile_out, const std::string& tag_in )
    {
        sLogProfile& profile_out = *p_profile_out;

        std::lock_guard<std::mutex> lock( g_state_mutex );
        const auto found = g_map_log_profile.find( tag_in );
        if( found == g_map_log_profile.end() )
        {
            return false;
        }
        profile_out = found->second;
        return true;
    }

    class SinkDispatchGuard final
    {
        public: SinkDispatchGuard( void ) { g_is_dispatching_sink = true; }
        public: ~SinkDispatchGuard( void ) { g_is_dispatching_sink = false; }
        public: SinkDispatchGuard( const SinkDispatchGuard& ) = delete;
        public: SinkDispatchGuard& operator = ( const SinkDispatchGuard& ) = delete;
    };

    static void dispatchLogRecord( const LogRecord& record_in,
                                   const sLogProfile& profile_in,
                                   const std::string& output_message_in )
    {
        std::vector<std::shared_ptr<LogSink>> sinks;
        {
            std::lock_guard<std::mutex> lock( g_state_mutex );
            if( g_minimum_log_level == LogLevel::Off ||
                record_in.level == LogLevel::Off ||
                static_cast<int>( record_in.level ) < static_cast<int>( g_minimum_log_level ) )
            {
                return;
            }
            sinks.reserve( g_log_sinks.size() );
            for( const auto& item : g_log_sinks )
            {
                sinks.push_back( item.second );
            }
        }

        {
            std::lock_guard<std::mutex> lock( g_output_mutex );
            switch( profile_in.mode )
            {
                case eLogMode::NONE:
                    break;
                case eLogMode::CONSOLE:
                    log::outputConsole( output_message_in );
                    break;
                case eLogMode::LOG_FILE:
                    log::outputLogFile( output_message_in, profile_in.fullpath );
                    break;
                case eLogMode::FULL:
                    log::outputConsole( output_message_in );
                    log::outputLogFile( output_message_in, profile_in.fullpath );
                    break;
                default:
                    break;
            }
        }

        // A sink may report its own failure through WSE. Suppress nested sink
        // dispatch on the same thread so the callback cannot recurse forever.
        if( g_is_dispatching_sink )
        {
            return;
        }

        SinkDispatchGuard guard;
        for( const std::shared_ptr<LogSink>& sink : sinks )
        {
            if( !sink )
            {
                continue;
            }
            try
            {
                sink->onLog( record_in );
            }
            catch( ... )
            {
                // Logging must not propagate sink failures into product code.
            }
        }
    }


    //! @brief 初期化.
    void ILogger::initalize( bool* p_can_output_out, std::string* p_message_out, const std::string& tag_in )
    {
        sLogProfile profile;
        if( !readLogProfile( &profile, tag_in ) )
        {
            return;
        }
        
        bool          &can_output = *p_can_output_out;
        std::string   &message    = *p_message_out   ;
        const LogLevel level = tag_in == WSE_DEV_TAG ? LogLevel::Debug : LogLevel::Info;
        LogLevel minimum_level;
        {
            std::lock_guard<std::mutex> lock( g_state_mutex );
            minimum_level = g_minimum_log_level;
        }
        if( profile.mode != eLogMode::NONE &&
            minimum_level != LogLevel::Off &&
            static_cast<int>( level ) >= static_cast<int>( minimum_level ) )
        {
            can_output =true;
        }
        message = "";
    }

    std::string formatLogMessage( const sLogConfig& config_in,
        const std::string& message_in, const std::string& file_in, int line_in )
    {
        const std::string log_tag = "<" + config_in.tag + ">";
        const std::string file_tag = ( file_in.empty() ) ? "" : "( " + extractFileName( file_in.c_str() ) + " ) ";
        const std::string line_tag = ( line_in == 0 ) ? ""    : getLineTag( line_in );
        const std::string pre = getLogPrefix( config_in.prefix );
        const std::string suf = getLogSuffix( config_in.suffix );

        return log_tag + pre + file_tag + line_tag + message_in + suf;
    }

    //! @brief 終了処理.
    void ILogger::finalize ( const std::string& tag_in, const std::string message_in, const std::string& file_in, const int line_in )
    {
        sLogProfile profile;
        if( !readLogProfile( &profile, tag_in ) )
        {
            return;
        }

        const sLogConfig config{ tag_in, profile.mode, profile.separate, profile.prefix, profile.suffix };
        const std::string log_message = formatLogMessage( config, message_in, file_in, line_in );
        LogRecord record;
        record.timestamp = getTimestampUtc();
        record.level = tag_in == WSE_DEV_TAG ? LogLevel::Debug : LogLevel::Info;
        record.tag = tag_in;
        record.source = tag_in;
        record.message = message_in;
        if( !file_in.empty() )
        {
            record.details = "file_in=" + extractFileName( file_in.c_str() ) +
                             " line_in=" + std::to_string( line_in );
        }
        dispatchLogRecord( record, profile, log_message );
    }

    
    //! @brief ビットシフトオペレーター.
    std::string ILogger::getMessageTip ( const std:: string& str, const std::string& tag )
    {
        sLogProfile profile;
        if( !readLogProfile( &profile, tag ) )
        {
            return "";
        }
        const eLogSeparate sep = profile.separate;
        std::string sep_str = getLogSeparator( sep );
        return sep_str + str;
    }

    //! @brief ビットシフトオペレーター.
    std::string ILogger::getMessageTip  ( const std::wstring& str_in, const std::string& tag_in )
    {
        return getMessageTip( wstring2UTF8( str_in ), tag_in );
    }

    //! @brief ビットシフトオペレーター.
    std::string ILogger::getMessageTip ( const char*    str_in, const std::string& tag_in )
    {
        return getMessageTip( std::string( str_in ), tag_in );
    }

    //! @brief ビットシフトオペレーター.
    std::string ILogger::getMessageTip  ( const wchar_t* str_in, const std::string& tag_in )
    {
        return getMessageTip( std::string( wstring2UTF8( std::wstring( str_in ) ) ), tag_in );
    }

    //! @brief ログを登録する.
    //! 
    //! @note
    //!  登録すると今のMAPサイズを超える場合は
    //!  サイズを一定の余裕をもって確保しなおす
    //! 
    void ILogger::registerLogConfig( const sLogConfig config_in )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        if( g_map_log_profile.find( config_in.tag ) != g_map_log_profile.end() )
        {
            return;
        }
        updateMapSize();

        sLogProfile profile;
        profile.separate = config_in.separate;
        profile.fullpath = normalizePath( config_in.path + "/" + config_in.name );
        profile.mode     = config_in.mode;
        profile.prefix   = config_in.prefix;
        profile.suffix   = config_in.suffix;

        const std::string tag = config_in.tag;
        g_map_log_profile[ tag ] = profile;
    }

    //! @brief ログを削除する.
    void ILogger::removeLogConfig  ( const std::string tag_in )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        if( g_map_log_profile.find( tag_in ) == g_map_log_profile.end() )
        {
            return;
        }
        g_map_log_profile.erase( tag_in );

    }

    //! @brief ログ構成を変更する.
    void ILogger::changeLogConfig( const sLogConfig config_in )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        if( g_map_log_profile.find( config_in.tag ) == g_map_log_profile.end() )
        {
            return;
        }

        sLogProfile profile;
        profile.separate = config_in.separate;
        profile.fullpath = normalizePath( config_in.path + "/" + config_in.name );
        profile.mode     = config_in.mode;
        profile.prefix   = config_in.prefix;
        profile.suffix   = config_in.suffix;

        const std::string tag = config_in.tag;
        g_map_log_profile[ tag ] = profile;
    
    }

    
    //! @brief ログ構成を変更する.
    void ILogger::changeLogConfig( const sLogConfig config_in, const std::string& fullpath_in )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        if( g_map_log_profile.find( config_in.tag ) == g_map_log_profile.end() )
        {
            return;
        }

        sLogProfile profile;
        profile.separate = config_in.separate;
        profile.fullpath = normalizePath( fullpath_in );
        profile.mode     = config_in.mode;
        profile.prefix   = config_in.prefix;
        profile.suffix   = config_in.suffix;

        const std::string tag = config_in.tag;
        g_map_log_profile[ tag ] = profile;
    }

    
    //! @brief ログを登録する.
    eLogMode ILogger::mode( const std::string& tag_in )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        if( g_map_log_profile.find( tag_in ) == g_map_log_profile.end() )
        {
            return eLogMode::NONE;
        }
        return g_map_log_profile[tag_in].mode;
    }
               
    //! @brief ログ構成を変更する.
    eLogSeparate ILogger::separator( const std::string& tag_in )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        if( g_map_log_profile.find( tag_in ) == g_map_log_profile.end() )
        {
            return eLogSeparate::SPACE;
        }
        return g_map_log_profile[tag_in].separate;
    }

    //! @brief ログ構成を変更する.
    eLogPrefix ILogger::prefix( const std::string& tag_in )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        if( g_map_log_profile.find( tag_in ) == g_map_log_profile.end() )
        {
            return eLogPrefix::NONE;
        }
        return g_map_log_profile[tag_in].prefix;
    }

    //! @brief ログを削除する.
    eLogSuffix ILogger::suffix  ( const std::string& tag_in )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        if( g_map_log_profile.find( tag_in ) == g_map_log_profile.end() )
        {
            return eLogSuffix::NONE;
        }
        return g_map_log_profile[tag_in].suffix;
    }
    
    //! @brief ログを削除する.
    std::string ILogger::fullpath  ( const std::string& tag_in )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        if( g_map_log_profile.find( tag_in ) == g_map_log_profile.end() )
        {
            return "";
        }
        return g_map_log_profile[tag_in].fullpath;
    }

    void setMinimumLogLevel( const LogLevel level_in )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        g_minimum_log_level = level_in;
    }

    LogLevel minimumLogLevel( void )
    {
        std::lock_guard<std::mutex> lock( g_state_mutex );
        return g_minimum_log_level;
    }

    LogSinkHandle registerLogSink( const std::shared_ptr<LogSink>& sink_in )
    {
        if( !sink_in )
        {
            return 0;
        }

        std::lock_guard<std::mutex> lock( g_state_mutex );
        const LogSinkHandle handle = g_next_sink_handle++;
        g_log_sinks.emplace( handle, sink_in );
        return handle;
    }

    void unregisterLogSink( const LogSinkHandle handle_in )
    {
        if( handle_in == 0 )
        {
            return;
        }
        std::lock_guard<std::mutex> lock( g_state_mutex );
        g_log_sinks.erase( handle_in );
    }

    static void writeStructuredLog( const LogLevel level_in,
                                    const std::string& profile_tag_in,
                                    const std::string& record_tag_in,
                                    const std::string& source_in,
                                    const std::string& message_in,
                                    const std::string& details_in )
    {
        sLogProfile profile;
        if( !readLogProfile( &profile, profile_tag_in ) )
        {
            profile.mode = eLogMode::NONE;
        }

        LogRecord record;
        record.timestamp = getTimestampUtc();
        record.level = level_in;
        record.tag = record_tag_in.empty() ? WSE_TAG : record_tag_in;
        record.source = source_in.empty() ? record.tag : source_in;
        record.message = message_in;
        record.details = details_in;

        std::ostringstream output;
        output << '[' << getStructuredLevelName( level_in ) << "] ["
               << record.tag << ']';
        if( record.source != record.tag )
        {
            output << " [" << record.source << ']';
        }
        output << ' ' << message_in;
        if( !details_in.empty() )
        {
            output << " | " << details_in;
        }
        output << END_LINE();

        dispatchLogRecord( record, profile, output.str() );
    }

    void writeLog( const LogLevel level_in,
                   const std::string& tag_in,
                   const std::string& source_in,
                   const std::string& message_in,
                   const std::string& details_in )
    {
        const std::string profile_tag = tag_in.empty() ? WSE_TAG : tag_in;
        writeStructuredLog( level_in, profile_tag, profile_tag, source_in, message_in, details_in );
    }

    void writeLog( const LogLevel level_in,
                   const std::string& tag_in,
                   const std::string& message_in,
                   const std::string& details_in )
    {
        const std::string record_tag = tag_in.empty() ? WSE_TAG : tag_in;
        writeStructuredLog( level_in, WSE_TAG, record_tag, record_tag, message_in, details_in );
    }

    void flushLog( void )
    {
        std::lock_guard<std::mutex> lock( g_output_mutex );
        std::cout.flush();
        std::cerr.flush();
    }



}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
