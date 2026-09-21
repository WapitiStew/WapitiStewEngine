//*****************************************************************************************************************
//! 
//! @file    wse_Log.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!   Aug-05, 2026   Add structured levels, records, sinks, and flush support.
//!   Aug-11, 2026   Separate application tags from feature sources.
//!   Sep-20, 2026   Keep application-specific tags and aliases in their owning application.
//!
//!
//! @brief
//!     \~japanese ログ出力や管理を行う汎用ユーティリティ。
//!     \~english  General-purpose utility for logging and log management.
//!
//! @details
//!     \~japanese
//!         ログをコンソールやファイルに出力できる構成可能なロガー。
//!         出力モード、区切り文字、接頭辞・接尾辞、ログファイルの場所などを指定可能。
//!         タグを指定してログ設定を一意に管理できる。
//!
//!     \~english
//!         A configurable logger that supports output to console and files.
//!         Allows setting output mode, separators, prefixes/suffixes, and file paths.
//!         Log configuration is uniquely managed via tags.
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
#ifndef WONDERSTEWENGINE_UTILITY_LOGGER_H
#define WONDERSTEWENGINE_UTILITY_LOGGER_H

#include<vector>
#include<string>
#include<sstream>
#include<cstdarg>
#include<cstdint>
#include<memory>

#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4244)  // ../
    #pragma warning(disable: 4464)  // ../
#endif
#include <utility>
#include "../depend/wse_Constant.h"
#include "../utility/wse_StringTool.h"
#include "../../dynamic.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif


#if defined(_MSC_VER)
#pragma warning(push)
    #pragma warning(disable: 4505)  //
    #pragma warning(disable: 4514)  //
    #pragma warning(disable: 4251)  // Dynamicライブラリでユーザー側に公開されないメンバーが含まれる
#endif
namespace wse
{
    //!
    //! @class eLogMode
    //! @brief
    //!     \~japanese ログ出力先を定義するモード。
    //!     \~english  Mode that defines log output destination.
    //!
    enum class WSE_API eLogMode : int
    {
        NONE        = 0, //!< \~japanese 出力しない.       \~english No output
        LOG_FILE    = 1, //!< \~japanese ファイルに出力.     \~english Output to file
        CONSOLE     = 2, //!< \~japanese コンソールに出力.   \~english Output to console
        FULL        = 3  //!< \~japanese 両方に出力.         \~english Output to both
    };
        
    //!
    //! @class eLogSeparate
    //! @brief
    //!     \~japanese ログ出力時の区切り文字の種類。
    //!     \~english  Separator type for log formatting.
    //!
    enum class WSE_API eLogSeparate : int
    {
        COMMA       = 0, //!< \~japanese カンマ.     \~english Comma
        SPACE       = 1, //!< \~japanese スペース.   \~english Space
        TAB         = 2, //!< \~japanese タブ.       \~english Tab
        COLON       = 3, //!< \~japanese コロン.     \~english Colon
        SEMICOLON   = 4  //!< \~japanese セミコロン. \~english Semicolon
    };

    //!
    //! @brief
    //!     \~japanese 区切り記号に対応する文字列を取得する。
    //!     \~english  Get the string representation of the log separator.
    //!
    //! @param[in] sep_in Separator type.
    //! @return Separator string.
    //!
    inline static std::string getLogSeparator( const eLogSeparate sep_in )
    {
        switch( sep_in )
        {
            case eLogSeparate::COMMA     : return ",";
            case eLogSeparate::SPACE     : return " ";
            case eLogSeparate::TAB       : return "\t";
            case eLogSeparate::COLON     : return ":";
            case eLogSeparate::SEMICOLON : return ";";
            default:break;
        }
        // ここには来ない.
        return "";
    }
        
    //!
    //! @class eLogPrefix
    //! @brief
    //!     \~japanese ログの先頭に付加する情報の種類。
    //!     \~english  Type of prefix added to log messages.
    //!
    enum class WSE_API eLogPrefix : int
    {
        NONE        = 0, //!< \~japanese なし.           \~english None
        TIME        = 1, //!< \~japanese 時間(HH:MM:SS). \~english Time (HH:MM:SS)
        DATE        = 2, //!< \~japanese 日付(YY/MM/DD). \~english Date (YY/MM/DD)
        FULL        = 3  //!< \~japanese 日時.           \~english Date and time
    };

    //!
    //! @class eLogSuffix
    //! @brief
    //!     \~japanese ログの末尾に付加する情報の種類。
    //!     \~english  Type of suffix added to log messages.
    //!
    enum class WSE_API eLogSuffix : int
    {
        NONE        = 0, //!< \~japanese なし.      \~english None
        LINE_BREAK  = 1  //!< \~japanese 改行あり.  \~english With line break
    };

    //!
    //! @brief
    //!     \~japanese ログ接尾語に対応する文字列を取得する。
    //!     \~english  Get the string representation of the log suffix.
    //!
    //! @param[in] sep_in Suffix type.
    //! @return Suffix string.
    //!
    inline static std::string getLogSuffix( const eLogSuffix sep_in )
    {
        switch( sep_in )
        {
            case eLogSuffix::LINE_BREAK : return END_LINE();
            case eLogSuffix::NONE       : return "";
            default:break;
        }
        // ここには来ない.
        return "";
    }

    //!
    //! @struct sLogConfig
    //! @brief
    //!     \~japanese ログの構成情報を保持する構造体。
    //!     \~english  Struct holding configuration information for logging.
    //!
    struct WSE_API sLogConfig
    {
        std::string    tag                 ;
        eLogMode       mode        ;
        eLogSeparate   separate   ;
        eLogPrefix     prefix      ;
        eLogSuffix     suffix      ;
        std::string    path;
        std::string    name;

        //! @brief Construct all members with explicit defaults.
        sLogConfig(
              const std::string& tag_in = ""
            , eLogMode mode_in = eLogMode::NONE
            , eLogSeparate separate_in = eLogSeparate::SPACE
            , eLogPrefix prefix_in = eLogPrefix::NONE
            , eLogSuffix suffix_in = eLogSuffix::NONE
            , const std::string& path_in = ""
            , const std::string& name_in = ""
        )
            : tag      ( tag_in )
            , mode     ( mode_in )
            , separate ( separate_in )
            , prefix   ( prefix_in )
            , suffix   ( suffix_in )
            , path     ( path_in )
            , name     ( name_in )
        {
        }
    };

    //!
    //! @enum LogLevel
    //! @brief
    //!     \~japanese 構造化ログの重大度と出力無効状態を表す。
    //!     \~english  Represents structured-log severity and the disabled state.
    //!
    enum class WSE_API LogLevel : int
    {
        Trace   = 0,
        Debug   = 1,
        Info    = 2,
        Warning = 3,
        Error   = 4,
        Off     = 5
    };

    //!
    //! @struct LogRecord
    //! @brief
    //!     \~japanese WSEが全出力先へ同一内容で配信する構造化ログレコード。
    //!     \~english  Structured record dispatched consistently to every WSE log destination.
    //!
    struct WSE_API LogRecord
    {
        std::string timestamp; //!< UTC ISO-8601 timestamp with millisecond precision.
        LogLevel level; //!< Log severity.
        std::string tag;       //!< Registered application log tag.
        std::string source;    //!< Component or subsystem name.
        std::string message;   //!< Human-readable summary.
        std::string details;   //!< Optional structured details.

        //! @brief Construct all members with explicit defaults.
        LogRecord(
              const std::string& timestamp_in = {}
            , const LogLevel& level_in = LogLevel::Info
            , const std::string& tag_in = {}
            , const std::string& source_in = {}
            , const std::string& message_in = {}
            , const std::string& details_in = {}
        )
            : timestamp ( timestamp_in )
            , level     ( level_in )
            , tag       ( tag_in )
            , source    ( source_in )
            , message   ( message_in )
            , details   ( details_in )
        {
        }
    };

    //!
    //! @class LogSink
    //! @brief
    //!     \~japanese 構造化ログを受け取るcallback interface。
    //!     \~english  Callback interface that receives structured log records.
    //!
    class WSE_API LogSink
    {
        public: virtual ~LogSink( void ) = default;
        public: virtual void onLog( const LogRecord& record ) = 0;
    };

    using LogSinkHandle = std::uint64_t;

    //! @brief \~japanese 標準Loggerと同じ書式で整形する。出力とSink配送は行わない。
    //! @brief \~english Format using the standard Logger layout without output or sink dispatch.
    //! @param[in] config_in Tag, timestamp prefix and line ending configuration.
    //! @param[in] message_in Message text.
    //! @param[in] file_in Producer source path; only its basename is emitted.
    //! @param[in] line_in Producer source line; zero omits the line label.
    //! @return Complete formatted record for a caller-owned asynchronous sink.
    WSE_API std::string formatLogMessage( const sLogConfig& config_in,
        const std::string& message_in, const std::string& file_in, int line_in );

    //! @brief 全WSE出力先へ適用する最小ログレベルを設定する。
    WSE_API void setMinimumLogLevel( LogLevel level_in );
    //! @brief 現在の最小ログレベルを取得する。
    WSE_API LogLevel minimumLogLevel( void );
    //! @brief LogSinkを登録し、解除に使用するhandleを返す。
    WSE_API LogSinkHandle registerLogSink( const std::shared_ptr<LogSink>& sink_in );
    //! @brief 登録済みLogSinkを解除する。
    WSE_API void unregisterLogSink( LogSinkHandle handle_in );
    //! @brief 登録済みtag_inと機能source_inを分離して構造化ログを配信する。
    WSE_API void writeLog( LogLevel level_in,
                           const std::string& tag_in,
                           const std::string& source_in,
                           const std::string& message_in,
                           const std::string& details_in );
    //! @brief 構造化ログをConsole、File、LogSinkへ1回ずつ配信する。
    //! @note 互換overloadでは第2引数をtag_inとsourceの両方として保持する。
    WSE_API void writeLog( LogLevel level_in,
                           const std::string& tag_in,
                           const std::string& message_in,
                           const std::string& details_in = "" );
    //! @brief 同期出力をflushし、呼出し前のログ出力完了を保証する。
    WSE_API void flushLog( void );
    
    //!
    //! @class ILogger
    //!
    //! @brief
    //!     \~japanese ログ構成およびメッセージ整形を管理する静的ユーティリティクラス。
    //!     \~english  Static utility class for managing log configuration and formatting.
    //!
    //! @details
    //!     \~japanese
    //!         ログのタグごとの出力方法（ファイル、コンソール、両方など）や
    //!         接頭辞、接尾辞、区切りなどの設定を行い、ログメッセージの整形や管理を行う。
    //!         また、ログ出力の初期化・終了、ログ設定の登録・変更・削除を提供する。
    //!
    //!     \~english
    //!         Manages log output settings (file, console, both) for each tag,
    //!         including prefix, suffix, and separators. Provides initialization,
    //!         finalization, registration, modification, and removal of log settings.
    //!
    class WSE_API ILogger
    {
        //!
        //! @brief
        //!     \~japanese 文字列とタグを使ってメッセージを整形する。
        //!     \~english  Format log message using string and tag.
        //!
        //! @param[in] str String
        //! @param[in] tag Logger TAG
        //! @return Format message
        //!
        public : static std::string getMessageTip( const std:: string& str, const std::string& tag );

        //!
        //! @brief
        //!     \~japanese ワイド文字列とタグを使ってメッセージを整形する。
        //!     \~english  Format log message using wide string and tag.
        //!
        //! @param[in] str_in Wide String
        //! @param[in] tag_in Logger TAG
        //! @return Format message
        //!
        public : static std::string getMessageTip( const std::wstring& str_in, const std::string& tag_in );

        //!
        //! @brief
        //!     \~japanese C文字列とタグを使ってメッセージを整形する。
        //!     \~english  Format log message using C-string and tag.
        //!
        //! @param[in] str_in  C-string
        //! @param[in] tag_in Logger TAG
        //! @return Format message
        //!
        public : static std::string getMessageTip( const char*         str_in, const std::string& tag_in );

        //!
        //! @brief
        //!     \~japanese ワイドC文字列とタグを使ってメッセージを整形する。
        //!     \~english  Format log message using wide C-string and tag.
        //!
        //! @param[in] str_in  Wide C-string
        //! @param[in] tag_in Logger TAG
        //! @return Format message
        //!
        public : static std::string getMessageTip( const wchar_t*      str_in, const std::string& tag_in );

        //!
        //! @brief
        //!     \~japanese ログ出力用のフラグとバッファを初期化する。
        //!     \~english  Initialize log output flag and buffer.
        //!
        //! @param[in] p_can_output_out Output flag pointer.
        //! @param[in] p_message_out Message  buffer pointer.
        //! @param[in] tag_in       Logger TAG
        //!
        public : static void initalize( bool* p_can_output_out, std::string* p_message_out, const std::string& tag_in );

        //!
        //! @brief
        //!     \~japanese ログ出力の終了処理を行う。
        //!     \~english  Finalize log output.
        //!
        //! @param[in] tag_in       Logger TAG
        //! @param[in] message_in   Output message
        //! @param[in] file_in      File name
        //! @param[in] line_in      Row number.
        //!
        public : static void finalize ( const std::string& tag_in, const std::string message_in, const std::string& file_in, const int line_in );

        //!
        //! @brief
        //!     \~japanese ログ設定を登録する。
        //!     \~english  Register log configuration.
        //!
        //! @param[in] config_in Log configuration.
        //!
        public : static void registerLogConfig( const sLogConfig config_in );

        //!
        //! @brief
        //!     \~japanese ログ設定を削除する。
        //!     \~english  Remove log configuration.
        //!
        //! @param[in] tag_in       Logger TAG
        //!
        public : static void removeLogConfig  ( const std::string tag_in );

        //!
        //! @brief
        //!     \~japanese ログ設定を変更する。
        //!     \~english  Change log configuration.
        //!
        //! @param[in] config_in New Log configuration.
        //!
        public : static void changeLogConfig( const sLogConfig config_in );

        //!
        //! @brief
        //!     \~japanese ログ設定とファイルパスを同時に変更する。
        //!     \~english  Change log configuration and file path.
        //!
        //! @param[in] config_in New Log configuration.
        //! @param[in] fullpath_in Full path of output directory.
        //!
        public : static void changeLogConfig( const sLogConfig config_in, const std::string& fullpath_in );

        //!
        //! @brief
        //!     \~japanese ログ出力モードを取得する。
        //!     \~english  Get log output mode.
        //!
        //! @param[in] tag_in       Logger TAG
        //! @return Output mode.
        //!
        public : static eLogMode mode( const std::string& tag_in );

        //!
        //! @brief
        //!     \~japanese 区切り記号の設定を取得する。
        //!     \~english  Get log separator setting.
        //!
        //! @param[in] tag_in       Logger TAG
        //! @return separator.
        //!
        public : static eLogSeparate separator( const std::string& tag_in );

        //!
        //! @brief
        //!     \~japanese 接頭語の設定を取得する。
        //!     \~english  Get log prefix setting.
        //!
        //! @param[in] tag_in       Logger TAG
        //! @return prefix setting.
        //!
        public : static eLogPrefix prefix( const std::string& tag_in );

        //!
        //! @brief
        //!     \~japanese 接尾語の設定を取得する。
        //!     \~english  Get log suffix setting.
        //!
        //! @param[in] tag_in       Logger TAG
        //! @return suffix setting.
        //!
        public : static eLogSuffix suffix  ( const std::string& tag_in );

        //!
        //! @brief
        //!     \~japanese ログファイルのフルパスを取得する。
        //!     \~english  Get full path of log file.
        //!
        //! @param[in] tag_in       Logger TAG
        //! @return Log file path.
        //!
        public : static std::string fullpath  ( const std::string& tag_in );

    };


    #if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4264)  // 
    #pragma warning(disable: 4626)  // 
    #pragma warning(disable: 4820)  // 
    #pragma warning(disable: 5046)  //     }
    #endif
    //!
    //! @class     Logger
    //!
    //! @brief
    //!     \~japanese ログ出力を管理するテンプレートクラス。
    //!     \~english  Template class for managing log output.
    //!
    //! @details
    //!     \~japanese
    //!         任意の文字列定数 `_TAG` を識別子として用い、個別のログ出力を管理するクラス。
    //!         出力先、接頭語、接尾語、区切り文字などの構成を `sLogConfig` によって柔軟に設定できる。
    //!         出力対象の有無は `m_can_output` によって動的に制御され、デストラクタで自動出力が行われる。
    //!         文字列・数値・列挙型など様々な形式に対する `operator<<` をオーバーロードし、ログ出力を簡潔に記述可能。
    //!     \~english
    //!         Template class to manage logging with an identifier `_TAG`.
    //!         Supports flexible configuration of output destination, prefix, suffix, and separator using `sLogConfig`.
    //!         Output condition is controlled via `m_can_output` and auto finalization occurs in the destructor.
    //!         Various `operator<<` overloads are provided for appending string, numeric, and enum values easily.
    //! 
    //! @note
    //!     \~japanese ログタグごとに異なる設定を行えるため、複数のモジュールで独立したログ出力が可能。
    //!     \~english  Enables separate log output configurations per tag for modular logging.
    //! 
    template < const char* _TAG >
    class Logger
    {
        //!
        //! @brief
        //!     \~japanese ログタグを取得する。
        //!     \~english  Get the log tag.
        //!
        //! @return TAG String.
        //!
        public : std::string tag()const{ return _TAG; }

        //!
        //! @brief
        //!     \~japanese ログタグを静的に取得する。
        //!     \~english  Get the log tag statically.
        //!
        //! @return TAG String.
        //!
        public : static std::string TAG(){ return _TAG; }

        private : std::string   m_message                ;   //!< \~english Log message buffer. \~japanese コンソール出力用のログ.
        private : bool          m_can_output             ;   //!< \~english Output flag. \~japanese コンソール出力用のログ.
        private : std::string   m_file                   ;   //!< \~english File name. \~japanese コンソール出力用のログ.
        private : int           m_line                   ;   //!< \~english Line number. \~japanese コンソール出力用のログ.

        //!
        //! @brief
        //!     \~japanese 出力可能かどうかを判定する。
        //!     \~english  Check whether output is enabled.
        //!
        //! @return Output flag  trye = can output, false = connot output.
        //!
        private: bool canOutput(){ return this->m_can_output; }

        //!
        //! @brief Default constructor.
        //!
        public : explicit Logger( void )
            : m_message    ()
            , m_can_output ( false )
            , m_file       ( "" )
            , m_line       ( 0 )
        {
            ILogger::initalize( &m_can_output, &m_message,  this->tag() );
        }

        //!
        //! @brief Constructor with file name and line number.
        //!
        //! @param[in] file_in File name.
        //! @param[in] line_in Line number.
        //!
        public : explicit Logger( const std::string& file_in, const int line_in )
            : m_message    ()
            , m_can_output ( false )
            , m_file       ( file_in )
            , m_line       ( line_in )
        {
            ILogger::initalize( &m_can_output, &m_message,  this->tag() );
        }

        //!
        //! @brief Destructor.
        //!
        public : virtual ~Logger( void )
        {
            if( this->canOutput() )
            {
                ILogger::finalize( this->tag(), m_message, m_file, m_line );
            }
        }


        //!
        //! @brief
        //!     \~japanese 文字列をログに追加するオペレータ。
        //!     \~english  Operator to append string to log.
        //!
        //! @param[in] str String
        //! @return Own reference.
        //!
        public : Logger& operator << ( const std:: string& str )
        { 
            if( this->canOutput() )
            { 
                this->m_message += ILogger::getMessageTip( str, this->tag() ); 
            } 
            return *this; 
        }

        //!
        //! @brief
        //!     \~japanese ワイド文字列をログに追加するオペレータ。
        //!     \~english  Operator to append wide string to log.
        //!
        //! @param[in] str Wide String
        //! @return Own reference.
        //!
        public : Logger& operator << ( const std::wstring& str )
        { 
            if( this->canOutput() )
            { 
                this->m_message += ILogger::getMessageTip( str, this->tag() ); 
            } 
            return *this; 
        }

        //!
        //! @brief
        //!     \~japanese C文字列をログに追加するオペレータ。
        //!     \~english  Operator to append C-style string to log.
        //!
        //! @param[in] str c-String
        //! @return Own reference.
        //!
        public : Logger& operator << ( const char*         str )
        { 
            if( this->canOutput() )
            { 
                this->m_message += ILogger::getMessageTip( str, this->tag() ); 
            } 
            return *this; 
        }

        //!
        //! @brief
        //!     \~japanese ワイドC文字列をログに追加するオペレータ。
        //!     \~english  Operator to append wide C-style string to log.
        //!
        //! @param[in] str Wide c-String
        //! @return Own reference.
        //!
        public : Logger& operator << ( const wchar_t*      str )
        { 
            if( this->canOutput() )
            { 
                this->m_message += ILogger::getMessageTip( str, this->tag() ); 
            } 
            return *this; 
        }

        // 算術型（数値型）専用のテンプレートオペレータ
        //!
        //! @brief
        //!     \~japanese 数値型の値をログに追加するオペレータ。
        //!     \~english  Operator to append arithmetic value to log.
        //!
        //! @tparam _Tp Numeric type template parameter.
        //! @param[in] val Value.
        //! @return Own reference.
        //!
        template <typename _Tp, std::enable_if_t<std::is_arithmetic<_Tp>::value, int> = 0>
        Logger& operator<<(const _Tp val) 
        {
            if (this->canOutput()) 
            {
                this->m_message += ILogger::getMessageTip(std::to_string(val), this->tag());
            }
            return *this;
        }

        // enum class 用テンプレートオペレータ
        //!
        //! @brief
        //!     \~japanese enum型の値を16進文字列としてログに追加するオペレータ。
        //!     \~english  Operator to append enum value as hex string to log.
        //!
        //! @tparam _Tp     Enum type template parameter.
        //! @param[in] val  Enum value
        //! @return Own reference.
        //!
        template < typename _Tp, std::enable_if_t<std::is_enum< _Tp >::value, int> = 0 >
        Logger& operator<<(const _Tp val) 
        {
            
            if (this->canOutput()) 
            {
                std::string tip = "";
                switch( sizeof( _Tp ) )
                {
                    case 1: tip = toHex( static_cast< uint8_t  >( val ) ); break;
                    case 2: tip = toHex( static_cast< uint16_t >( val ) ); break;
                    case 4: tip = toHex( static_cast< uint32_t >( val ) ); break;
                    case 8: tip = toHex( static_cast< uint64_t >( val ) ); break;
                    default : tip = toHex( static_cast< uint64_t >( val ) );
                }
                this->m_message += ILogger::getMessageTip( tip, this->tag());
            }
            return *this;
        }

        // bool型用オペレータ
        //!
        //! @brief
        //!     \~japanese bool値をログに追加するオペレータ。
        //!     \~english  Operator to append boolean value to log.
        //!
        //! @param[in] val Boolean value
        //! @return Own reference.
        //!
        Logger& operator<< ( const bool val ) 
        {
            
            if (this->canOutput()) 
            {
                std::string tip = ( val ) ? "true" : "false" ;
                this->m_message += ILogger::getMessageTip( tip, this->tag());
            }
            return *this;
        }

        //!
        //! @brief
        //!     \~japanese ログ構成を登録する。
        //!     \~english  Register log configuration.
        //!
        //! @param[in] config_in   Log configuration.
        //!
        public : static void registerLogConfig( const sLogConfig config_in )
        {
            ILogger::registerLogConfig( config_in );
        }

        //!
        //! @brief
        //!     \~japanese ログ構成を削除する。
        //!     \~english  Remove log configuration.
        //!
        public : static void removeLogConfig  ( void )
        {
            ILogger::removeLogConfig( TAG() );
        }

        //!
        //! @brief
        //!     \~japanese ログ構成を変更する。
        //!     \~english  Change log configuration.
        //!
        //! @param[in] config_in   Log configuration.
        //!
        public : static void changeLogConfig( const sLogConfig config_in )
        {
            ILogger::changeLogConfig( config_in );
        }

        //!
        //! @brief
        //!     \~japanese ログ構成を変更する（パス指定付き）。
        //!     \~english  Change log configuration with path.
        //!
        //! @param[in] config_in New Log configuration.
        //! @param[in] fullpath_in Full path of output directory.
        //!
        public : static void changeLogConfig( const sLogConfig config_in, const std::string& fullpath_in )
        {
            ILogger::changeLogConfig( config_in, fullpath_in );
        }

        //!
        //! @brief
        //!     \~japanese 現在のログモードを取得する。
        //!     \~english  Get current log mode.
        //!
        //! @return Output mode.
        //!
        public : static eLogMode mode(  )
        {
            return ILogger::mode( _TAG );
        }

        //!
        //! @brief
        //!     \~japanese 区切り文字設定を取得する。
        //!     \~english  Get log separator setting.
        //!
        //! @return separator.
        //!
        public : static eLogSeparate separator(  )
        {
            return ILogger::separator( _TAG );
        }

        //!
        //! @brief
        //!     \~japanese 接頭辞設定を取得する。
        //!     \~english  Get log prefix setting.
        //!
        //! @return prefix setting.
        //!
        public : static eLogPrefix prefix(  )
        {
            return ILogger::prefix( _TAG );
        }

        //!
        //! @brief
        //!     \~japanese 接尾辞設定を取得する。
        //!     \~english  Get log suffix setting.
        //!
        //! @return suffix setting.
        //!
        public : static eLogSuffix suffix(  )
        {
            return ILogger::suffix( _TAG );
        }

        //!
        //! @brief
        //!     \~japanese ログファイルのフルパスを取得する。
        //!     \~english  Get full path of the log file.
        //!
        //! @return Log file path.
        //!
        public : static std::string fullpath(  )
        {
            return ILogger::fullpath( _TAG );
        }

    };
    #if defined(_MSC_VER)
    #pragma warning(pop)
    #endif

    // constexprで文字列リテラルを指定
    extern WSE_API const char WSE_TAG[];
    typedef Logger< WSE_TAG >  WLog;
    #define DWLog() Logger< wse::WSE_TAG >( __FILE__, __LINE__ )

    //!
    //! @brief
    //!     \~japanese デフォルトのログ設定を登録します。
    //!     \~english  Register the default log configuration.
    //!
    //! @details
    //!     \~japanese
    //!         ログ出力をコンソールに設定し、スペース区切り、プレフィックス・サフィックスを付加した設定を登録します。
    //!
    //!     \~english
    //!         Registers default log settings to output to console, with space-separated format, full prefix, and line break suffix.
    //!
    inline static void registDefaultLog( void ) 
    {
       WLog::registerLogConfig( {
              WLog::TAG(), eLogMode::CONSOLE, eLogSeparate::SPACE
            , eLogPrefix::FULL, eLogSuffix::LINE_BREAK 
        } );
    }
    
    //!
    //! @brief
    //!     \~japanese ログモードおよび出力先ファイルのパス・名前を変更します。
    //!     \~english  Change log mode and output destination path and file name.
    //!
    //! @details
    //!     \~japanese
    //!         指定されたモード、ファイルパス、ファイル名に基づき、ログ出力設定を変更します。
    //!
    //!     \~english
    //!         Updates log configuration based on the specified mode, file path, and file name.
    //!
    //! @param[in] mode_in Log output mode.
    //! @param[in] path_in Path to output log file.
    //! @param[in] name_in File name for log.
    //!
    inline static void changeDefaultLog( const eLogMode mode_in, const std::string& path_in = "", const std::string& name_in = ""  ) 
    {
        WLog::changeLogConfig(
            { WLog::TAG(), mode_in, WLog::separator(), WLog::prefix(), WLog::suffix(), path_in, name_in }
        );
    }

    //!
    //! @brief
    //!     \~japanese ログの区切り形式を変更します。
    //!     \~english  Change the log separator format.
    //!
    //! @details
    //!     \~japanese
    //!         区切り記号のみを変更し、それ以外のログ設定は現在の構成を維持します。
    //!
    //!     \~english
    //!         Changes only the log separator while preserving current logging configuration.
    //!
    //! @param[in] separater_in Separator format.
    //!
    inline static void changeDefaultLog( const eLogSeparate separater_in ) 
    {
        WLog::changeLogConfig({ WLog::TAG(), WLog::mode(), separater_in, WLog::prefix(), WLog::suffix() }, WLog::fullpath() );
    }

    //!
    //! @brief
    //!     \~japanese ログのプレフィックス形式を変更します。
    //!     \~english  Change the log prefix format.
    //!
    //! @details
    //!     \~japanese
    //!         プレフィックス部分のみを変更し、それ以外のログ設定は現在の構成を維持します。
    //!
    //!     \~english
    //!         Changes only the log prefix while preserving other settings.
    //!
    //! @param[in] pre_in Prefix format.
    //!
    inline static void changeDefaultLog( const eLogPrefix pre_in ) 
    {
        WLog::changeLogConfig(
              { WLog::TAG(), WLog::mode(), WLog::separator(), pre_in, WLog::suffix() }, WLog::fullpath()
        );
    }

    //!
    //! @brief
    //!     \~japanese ログのサフィックス形式を変更します。
    //!     \~english  Change the log suffix format.
    //!
    //! @details
    //!     \~japanese
    //!         サフィックス部分のみを変更し、それ以外のログ設定は現在の構成を維持します。
    //!
    //!     \~english
    //!         Changes only the log suffix while preserving other settings.
    //!
    //! @param[in] suffix_in Suffix format.
    //!
    inline static void changeDefaultLog( const eLogSuffix suffix_in ) 
    {
        WLog::changeLogConfig(
              { WLog::TAG(), WLog::mode(), WLog::separator(), WLog::prefix(), suffix_in }, WLog::fullpath()
        );
    }


    //! 
    //! @def WSE_DEV_TAG
    //! @brief
    //!     \~japanese 開発用ログに使用する定数タグ。
    //!     \~english  Constant tag for development logging.
    extern WSE_API const char WSE_DEV_TAG[];

    //! 
    //! @typedef DLog
    //! @brief
    //!     \~japanese 開発用ログクラスの別名定義。
    //!     \~english  Alias for development logger class.
    typedef Logger< WSE_DEV_TAG > DLog;

    //! 
    //! @def DDLog
    //! @brief
    //!     \~japanese 開発用ログ出力用マクロ（ファイル名と行番号付き）。
    //!     \~english  Development log macro with file and line information.
    #define DDLog() Logger< wse::WSE_DEV_TAG >( __FILE__, __LINE__ )

    //! 
    //! @brief
    //!     \~japanese 開発用のデフォルトログ構成を登録する。
    //!     \~english  Register default configuration for development log.
    //!
    inline static void registDevelopLog( void ) 
    {
        DLog::registerLogConfig( { 
              DLog::TAG(), eLogMode::CONSOLE, eLogSeparate::SPACE
            , eLogPrefix::FULL, eLogSuffix::LINE_BREAK 
            } );
    }
    //! 
    //! @brief
    //!     \~japanese 開発用ログの出力モードを変更する。
    //!     \~english  Change output mode for development log.
    //!
    inline static void changeDevelopLog( const eLogMode mode_in, const std::string& path_in = "", const std::string& name_in = "" ) 
    {
        DLog::changeLogConfig( 
            { DLog::TAG(), mode_in, DLog::separator(), DLog::prefix(), DLog::suffix(), path_in, name_in }
        );
    }

    //! 
    //! @brief
    //!     \~japanese 開発用ログの区切り記号を変更する。
    //!     \~english  Change separator for development log.
    //! 
    inline static void changeDevelopLog( const eLogSeparate separater_in ) 
    {
        DLog::changeLogConfig( { DLog::TAG(), DLog::mode(), separater_in, DLog::prefix(), DLog::suffix() }, DLog::fullpath() );
    }

    //! 
    //! @brief
    //!     \~japanese 開発用ログの接頭語を変更する。
    //!     \~english  Change prefix for development log.
    //! 
    inline static void changeDevelopLog( const eLogPrefix prefix_in ) 
    {
        DLog::changeLogConfig({ DLog::TAG(), DLog::mode(), DLog::separator(), prefix_in, DLog::suffix() }, DLog::fullpath() );
    }

    //! 
    //! @brief
    //!     \~japanese 開発用ログの接尾語を変更する。
    //!     \~english  Change suffix for development log.
    //! 
    inline static void changeDevelopLog( const eLogSuffix suffix_in ) 
    {
        DLog::changeLogConfig({ DLog::TAG(), DLog::mode(), DLog::separator(), DLog::prefix(), suffix_in }, DLog::fullpath() );
    }


    //! 
    //! @def Log_t
    //! @brief
    //!     \~japanese 任意タグ付きのログを生成するマクロ。
    //!     \~english  Macro for creating logger with specified tag.
    //! 
    #define  Log_t( TAG ) Logger< TAG >()

    //! 
    //! @def DLog_t
    //! @brief
    //!     \~japanese 任意タグ付きのログ（ファイル・行番号付き）を生成するマクロ。
    //!     \~english  Macro for creating logger with specified tag, file, and line.
    //! 
    #define DLog_t( TAG ) Logger< TAG>( __FILE__, __LINE__ )

    //! 
    //! @brief
    //!     \~japanese デフォルト構成のログを登録する。
    //!     \~english  Register default log configuration.
    //!
    template < const char* TAG >
    inline static void registLog() 
    {
        wse::Logger< TAG >::registerLogConfig( {
              wse::Logger< TAG >::TAG(), wse::eLogMode::CONSOLE, wse::eLogSeparate::SPACE
            , wse::eLogPrefix::FULL, wse::eLogSuffix::LINE_BREAK 
         } );
    }

    //! 
    //! @brief
    //!     \~japanese 出力モードを変更する。
    //!     \~english  Change the log output mode.
    //!
    template < const char* TAG >
    inline static void changeLog( const eLogMode mode_in, const std::string& path_in = "", const std::string& name_in = "" ) 
    {
        Logger< TAG >::changeLogConfig( 
            { Logger< TAG >::TAG(), mode_in, Logger< TAG >::separator(), Logger< TAG >::prefix(), Logger< TAG >::suffix(), path_in, name_in }
        );
    }

    //! 
    //! @brief
    //!     \~japanese 区切り記号を変更する。
    //!     \~english  Change log separator.
    //!
    template < const char* TAG >
    inline static void changeLog( const eLogSeparate separater_in ) 
    {
        Logger< TAG >::changeLogConfig( { Logger< TAG >::TAG(), Logger< TAG >::mode(), separater_in, Logger< TAG >::prefix(), Logger< TAG >::suffix() }, Logger< TAG >::fullpath() );
    }

    //! 
    //! @brief
    //!     \~japanese 接頭語を変更する。
    //!     \~english  Change log prefix.
    //!
    template < const char* TAG >
    inline static void changeLog( const eLogPrefix prefix_in ) 
    {
        Logger< TAG >::changeLogConfig({ Logger< TAG >::TAG(), Logger< TAG >::mode(), Logger< TAG >::separator(), prefix_in, Logger< TAG >::suffix() }, Logger< TAG >::fullpath() );
    }

    //! 
    //! @brief
    //!     \~japanese 接尾語を変更する。
    //!     \~english  Change log suffix.
    //!
    template < const char* TAG >
    inline static void changeLog( const eLogSuffix suffix_in ) 
    {
        Logger< TAG >::changeLogConfig({ Logger< TAG >::TAG(), Logger< TAG >::mode(), Logger< TAG >::separator(), Logger< TAG >::prefix(), suffix_in }, Logger< TAG >::fullpath() );
    }
};




#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif  //WONDERSTEWENGINE_UTILITY_LOGGER_H
