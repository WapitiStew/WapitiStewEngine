//*****************************************************************************************************************
//!
//! @file    capi_internal.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese 平坦C ABI実装が共有する内部Helper. 公開Headerではない.
//! @brief   \~english  Internal helpers shared by the flat C ABI implementation; not a public header.
//!
//! @details
//!     \~japanese
//!     @n 失敗MessageはComponentを跨いで同じ呼出Threadに保持する必要があるため、Status生成と
//!        Exception boundaryを本Headerへ集約する。
//!     \~english
//!     @n The failure message must live on one calling thread across every component, so status
//!        construction and the exception boundary are centralized here.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_LANG_CS_CAPI_INTERNAL_H
#define WONDERSTEWENGINE_LANG_CS_CAPI_INTERNAL_H

#include <wse/capi/wse_capi.h>
#include <wse/capi/wse_capi_core.h>

#include <wse/binding/Cancellation.h>
#include <wse/binding/Error.h>
#include <wse/binding/FrameBuffer.h>
#include <wse/binding/Runtime.h>

#if defined( WSE_HAS_XPT )
#include <xpt/operation/Cancellation.h>
#endif

#include <cstddef>
#include <cstdint>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace wse
{
namespace capi
{

//! \~japanese 呼出Thread単位の失敗Messageを設定してStatusを組み立てる.
//! \~english  Records the thread-local failure message and builds a status.
wse_capi_status makeStatus(
      const wse_capi_error_category category_in
    , const std::int32_t code_in
    , const std::string_view& message_in
    , const std::int64_t native_code_in = 0 ) noexcept;

//! \~japanese 成功Statusを返し、失敗Messageを消去する.
//! \~english  Returns a success status and clears the failure message.
wse_capi_status makeSuccess() noexcept;

//! \~japanese 引数契約違反のStatusを返す.
//! \~english  Returns a status for an argument-contract violation.
wse_capi_status makeInvalidArgument( const std::string_view& message_in ) noexcept;

//! \~japanese `wse::binding::Error`をC ABIのStatusへ変換する.
//! \~english  Converts a `wse::binding::Error` into a C ABI status.
wse_capi_status fromBindingError( const binding::Error& error_in );

//! \~japanese UTF-8文字列を2回呼出方式で複製する.
//! \~english  Copies a UTF-8 string using the two-call pattern.
wse_capi_status copyString(
      char*              p_buffer_out
    , std::size_t*       p_size_out
    , const std::string& source_in
    , const std::size_t  capacity_in );

//! \~japanese 現在の失敗Messageを返す.
//! \~english  Returns the current failure message.
const char* lastErrorMessage() noexcept;

//!
//! \~japanese
//! @brief   例外境界. C ABIからC++例外を漏らさない.
//! @details Header内に置くのはTemplateだからであり、状態は`capi_internal.cpp`側だけが持つ.
//! \~english
//! @brief   Exception boundary that prevents C++ exceptions from escaping the C ABI.
//! @details It lives in the header only because it is a template; state stays in `capi_internal.cpp`.
//!
template <typename Operation>
wse_capi_status guard( Operation&& operation_in ) noexcept
{
    try
    {
        return operation_in();
    }
    catch ( const std::bad_alloc& )
    {
        return makeStatus( WSE_CAPI_ERROR_RESOURCE_EXHAUSTED, 0, "Out of memory." );
    }
    catch ( const std::exception& exception_in )
    {
        return makeStatus( WSE_CAPI_ERROR_INTERNAL, 0, exception_in.what() );
    }
    catch ( ... )
    {
        return makeStatus( WSE_CAPI_ERROR_INTERNAL, 0, "Unknown native failure." );
    }
}

} // namespace capi
} // namespace wse

//! \~japanese 不透明Handleの前方宣言に対応するC ABIの実体. 全Component単位が共有する.
//! \~english  Concrete bodies behind the opaque C ABI handles, shared by every component unit.
struct wse_capi_runtime_t final
{
    wse::binding::Runtime runtime;
};

struct wse_capi_frame_buffer_t final
{
    wse::binding::FrameBuffer buffer;

    explicit wse_capi_frame_buffer_t( wse::binding::FrameBuffer buffer_in )
        : buffer ( std::move( buffer_in ) )
    {
    }
};

//!
//! \~japanese
//! @brief   C ABIのCancellation Handle.
//! @details Coreの待機とXPT Operationは別のCancellation型を使う。呼出元が扱う取消の概念を1つに
//!          保つため、本Handleは両方のSourceを所有し、`cancel`で同時に取り消す。
//! \~english
//! @brief   Cancellation handle of the C ABI.
//! @details Core waits and XPT operations use distinct cancellation types. To keep one cancellation
//!          concept for callers, this handle owns both sources and cancels them together.
//!
struct wse_capi_cancellation_t final
{
    wse::binding::CancellationSource source;
#if defined( WSE_HAS_XPT )
    wse::xpt::CancellationSource transport_source;
#endif

    void cancelAll() noexcept
    {
        this->source.cancel();
#if defined( WSE_HAS_XPT )
        this->transport_source.cancel();
#endif
    }
};

#endif // WONDERSTEWENGINE_LANG_CS_CAPI_INTERNAL_H
