//*****************************************************************************************************************
//!
//! @file    HttpClient.h
//! @brief   \~japanese Library非依存の同期HTTP Client契約を定義する.
//! @brief   \~english  Defines a library-independent synchronous HTTP client contract.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-26, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#pragma once

#ifndef WONDERSTEWENGINE_XPT_HTTP_HTTPCLIENT_H
#define WONDERSTEWENGINE_XPT_HTTP_HTTPCLIENT_H

#include "../error/TransportError.h"
#include "../operation/OperationContext.h"
#include "../retry/RetryPolicy.h"
#include "../../dynamic.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wse
{
namespace xpt
{

//! \~japanese 対応HTTP Method. \~english Supported HTTP methods.
enum class eHttpMethod : std::uint8_t
{
      Get = 0
    , Head
    , Post
    , Put
    , Patch
    , Delete
};

//! \~japanese HTTP Serverが提示した方式からBackendが認証方式を選択する明示Policy.
//! \~english  Explicit policy that lets the backend select a server-advertised HTTP authentication scheme.
enum class eHttpAuthenticationPolicy : std::uint8_t
{
    ServerNegotiated = 0
};

//! \~japanese Request Headerへ秘密値を埋め込まない、Move-onlyのHTTP認証値.
//! \~english  Move-only HTTP authentication value that keeps secrets out of request headers.
class WSE_API HttpAuthentication final
{
  private:
    eHttpAuthenticationPolicy m_policy;
    std::string m_username;
    std::string m_secret;

  public:
    HttpAuthentication(
          eHttpAuthenticationPolicy policy_in
        , const std::string& username_in
        , const std::string& secret_in );
    ~HttpAuthentication();
    HttpAuthentication( HttpAuthentication&& other_inout ) noexcept;
    HttpAuthentication& operator=( HttpAuthentication&& other_inout ) noexcept;
    HttpAuthentication( const HttpAuthentication& ) = delete;
    HttpAuthentication& operator=( const HttpAuthentication& ) = delete;

    eHttpAuthenticationPolicy policy() const noexcept;
    const std::string& username() const noexcept;
    const std::string& secret() const noexcept;
    bool isValid() const noexcept;
};

//! \~japanese 1個のHTTP Header. \~english One HTTP header field.
struct WSE_API sHttpHeader final
{
    std::string name;  //!< Header field name.
    std::string value; //!< Header field value.

    //! \~japanese 空Headerを生成する. \~english Creates an empty header.
    sHttpHeader();

    //! @param [in] name_in Header field名.
    //! @param [in] value_in Header field値.
    sHttpHeader( const std::string& name_in, const std::string& value_in );
};

//! \~japanese HTTP Request値. \~english HTTP request value.
class WSE_API HttpRequest final
{
  private:
    eHttpMethod m_method;
    std::string m_url;
    std::vector<sHttpHeader> m_headers;
    std::vector<std::uint8_t> m_body;

  public:
    //! @param [in] method_in HTTP Method.
    //! @param [in] url_in `http://`または`https://` Absolute URL.
    HttpRequest( const eHttpMethod method_in, const std::string& url_in );

    //! @return HTTP Method.
    eHttpMethod method() const noexcept;

    //! @return Absolute URL.
    const std::string& url() const noexcept;

    //! @return Request Header一覧. 同名Headerの順序を保持する.
    const std::vector<sHttpHeader>& headers() const noexcept;

    //! @return Request Body [byte].
    const std::vector<std::uint8_t>& body() const noexcept;

    //! \~japanese Headerを末尾へ追加する. \~english Appends a header field.
    //! @param [in] name_in Header field名.
    //! @param [in] value_in Header field値.
    void addHeader( const std::string& name_in, const std::string& value_in );

    //! \~japanese Request Bodyを設定する. \~english Sets the request body.
    //! @param [in] body_in Body [byte].
    void setBody( const std::vector<std::uint8_t>& body_in );
};

//! \~japanese HTTP Response値. Error時も受信済みStatus／Header／Bodyを保持できる.
//! \~english  HTTP response value that can preserve received data on failure.
class WSE_API HttpResponse final
{
  private:
    std::uint16_t m_status_code;
    std::vector<sHttpHeader> m_headers;
    std::vector<std::uint8_t> m_body;
    std::uint32_t m_attempt_count;

  public:
    //! \~japanese 未受信Responseを生成する. \~english Creates an empty response.
    HttpResponse() noexcept;

    //! @param [in] status_code_in HTTP Status Code. 未受信時0.
    //! @param [in] headers_in Response Header一覧.
    //! @param [in] body_in Response Body [byte].
    //! @param [in] attempt_count_in 実行済み試行数.
    HttpResponse(
          const std::uint16_t             status_code_in
        , const std::vector<sHttpHeader>& headers_in
        , const std::vector<std::uint8_t>& body_in
        , const std::uint32_t             attempt_count_in
    );

    //! @return HTTP Status Code. 未受信時0.
    std::uint16_t status_code() const noexcept;

    //! @return Response Header一覧.
    const std::vector<sHttpHeader>& headers() const noexcept;

    //! @return Response Body [byte].
    const std::vector<std::uint8_t>& body() const noexcept;

    //! @return 初回を含む実行済み試行数.
    std::uint32_t attempt_count() const noexcept;
};

//! \~japanese HTTP交換の部分進捗契約. 4xx/5xxはErrorと並んでResponseが意味を持つ.
//! \~english  Partial-progress contract for an HTTP exchange: on a 4xx/5xx the response stays
//!            meaningful alongside the error.
using HttpResult = TransferResult< HttpResponse >;

//! \~japanese HTTP実行上限と明示Retry契約. \~english HTTP execution limits and explicit retry contract.
class WSE_API HttpExecutionOptions final
{
  private:
    std::size_t m_maximum_response_body_size;
    RetryPolicy m_retry_policy;
    eRetryOperationSafety m_operation_safety;
    bool m_retry_transient_failures;

  public:
    //! \~japanese 8 MiB上限、Retry無効、安全側の非冪等Optionを生成する.
    //! \~english  Creates an 8 MiB, no-retry, fail-safe non-idempotent option set.
    HttpExecutionOptions() noexcept;

    //! @param [in] maximum_response_body_size_in Response Body上限 [byte]. 1以上.
    //! @param [in] retry_policy_in Retry回数／Backoff Policy.
    //! @param [in] operation_safety_in 呼出側が表明するRequest再実行安全性.
    //! @param [in] retry_transient_failures_in 一時的Transport／HTTP FailureをRetry候補にする場合true.
    HttpExecutionOptions(
          const std::size_t             maximum_response_body_size_in
        , const RetryPolicy&            retry_policy_in
        , const eRetryOperationSafety   operation_safety_in
        , const bool                    retry_transient_failures_in
    ) noexcept;

    //! @return Response Body上限 [byte].
    std::size_t maximum_response_body_size() const noexcept;

    //! @return Retry Policy.
    const RetryPolicy& retry_policy() const noexcept;

    //! @return 呼出側が表明したRequest再実行安全性.
    eRetryOperationSafety operation_safety() const noexcept;

    //! @return 一時的FailureをRetry候補とする場合true.
    bool shouldRetryTransientFailures() const noexcept;

    //! @return 全Optionが有効な場合true.
    bool isValid() const noexcept;
};

//! \~japanese Caller-confinedの同期HTTP Client. Backend型を公開しない.
//! \~english  Caller-confined synchronous HTTP client that hides its backend types.
class WSE_API HttpClient final
{
  public:
    //! \~japanese HTTP Requestを同期実行する.
    //! \~english  Executes an HTTP request synchronously.
    //! @param [in] request_in Method、URL、HeaderおよびBody.
    //! @param [in] options_in Body上限と明示Retry契約.
    //! @param [in] context_in 全試行とBackoffを含むTimeout／Cancellation.
    //! @return ResponseとStructured Error. HTTP 4xx／5xxはResponseを保持したError.
    HttpResult execute(
          const HttpRequest&          request_in
        , const HttpExecutionOptions& options_in
        , const OperationContext&     context_in
    ) const;

    //! \~japanese Server提示方式を用いてHTTP Requestを同期実行する.
    //! \~english  Executes an HTTP request with an explicitly server-negotiated authentication policy.
    //! @param [in] request_in Method、URL、HeaderおよびBody.
    //! @param [in] options_in Body上限と明示Retry契約.
    //! @param [in] authentication_in Runtimeで解決済みのMove-only認証値.
    //! @param [in] context_in 全試行とBackoffを含むTimeout／Cancellation.
    //! @return ResponseとStructured Error. HTTP 4xx／5xxはResponseを保持したError.
    HttpResult executeAuthenticated(
          const HttpRequest&          request_in
        , const HttpExecutionOptions& options_in
        , const HttpAuthentication&   authentication_in
        , const OperationContext&     context_in
    ) const;
};

} // namespace xpt
} // namespace wse

#endif // WONDERSTEWENGINE_XPT_HTTP_HTTPCLIENT_H
