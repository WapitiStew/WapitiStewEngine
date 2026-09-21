//*****************************************************************************************************************
//!
//! @file    wse_capi_xpt.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese XPT Transport(TCP、UDP、Serial、HTTP)の平坦C ABI.
//! @brief   \~english  Flat C ABI for the XPT transports: TCP, UDP, serial, and HTTP.
//!
//! @details
//!     \~japanese
//!     @n XPTはDefault Timeoutを持たない。全Operationが`wse_capi_operation_context`を要求するため、
//!        呼出元が無限待機へ落ちることはない。
//!     @n 受信Byte列は`wse_capi_frame_buffer`として返し、呼出元が`wse_capi_frame_buffer_destroy`で解放する。
//!     @n Statusの`code`はXPTの安定Code(`wse_capi_transport_error_code`)を保持する。Categoryは共通
//!        Binding Categoryへ正規化する。
//!     @n 本ABIは正準のXPT APIのみを公開する。
//!
//!     \~english
//!     @n XPT has no default timeout. Every operation requires a `wse_capi_operation_context`, so a
//!        caller can never fall into an unbounded wait.
//!     @n Received bytes are returned as a `wse_capi_frame_buffer` that the caller releases with
//!        `wse_capi_frame_buffer_destroy`.
//!     @n A status `code` preserves the stable XPT code (`wse_capi_transport_error_code`) while the
//!        category is normalized to the common binding categories.
//!     @n This ABI exposes the canonical XPT API only.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_CAPI_XPT_H
#define WONDERSTEWENGINE_WSE_CAPI_XPT_H

#include "wse_capi.h"
#include "wse_capi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

//! \~japanese `wse::xpt::eTransportErrorCategory`と同じ値.
//! \~english  Same values as `wse::xpt::eTransportErrorCategory`.
typedef enum wse_capi_transport_error_category
{
      WSE_CAPI_TRANSPORT_CATEGORY_NONE         = 0
    , WSE_CAPI_TRANSPORT_CATEGORY_VALIDATION   = 1
    , WSE_CAPI_TRANSPORT_CATEGORY_RESOLUTION   = 2
    , WSE_CAPI_TRANSPORT_CATEGORY_CONNECTION   = 3
    , WSE_CAPI_TRANSPORT_CATEGORY_INPUT_OUTPUT = 4
    , WSE_CAPI_TRANSPORT_CATEGORY_TIMEOUT      = 5
    , WSE_CAPI_TRANSPORT_CATEGORY_CANCELLATION = 6
    , WSE_CAPI_TRANSPORT_CATEGORY_PROTOCOL     = 7
    , WSE_CAPI_TRANSPORT_CATEGORY_SECURITY     = 8
    , WSE_CAPI_TRANSPORT_CATEGORY_HTTP         = 9
} wse_capi_transport_error_category;

//! \~japanese `wse::xpt::eTransportErrorCode`と同じ値. Statusの`code`が保持する.
//! \~english  Same values as `wse::xpt::eTransportErrorCode`, preserved in a status `code`.
typedef enum wse_capi_transport_error_code
{
      WSE_CAPI_TRANSPORT_NONE                 = 0
    , WSE_CAPI_TRANSPORT_INVALID_ARGUMENT     = 1
    , WSE_CAPI_TRANSPORT_HOST_NOT_FOUND       = 2
    , WSE_CAPI_TRANSPORT_ADDRESS_UNAVAILABLE  = 3
    , WSE_CAPI_TRANSPORT_CONNECTION_REFUSED   = 4
    , WSE_CAPI_TRANSPORT_CONNECTION_RESET     = 5
    , WSE_CAPI_TRANSPORT_NETWORK_UNREACHABLE  = 6
    , WSE_CAPI_TRANSPORT_NOT_CONNECTED        = 7
    , WSE_CAPI_TRANSPORT_REMOTE_CLOSED        = 8
    , WSE_CAPI_TRANSPORT_TIMED_OUT            = 9
    , WSE_CAPI_TRANSPORT_CANCELLED            = 10
    , WSE_CAPI_TRANSPORT_BIND_FAILED          = 11
    , WSE_CAPI_TRANSPORT_SEND_FAILED          = 12
    , WSE_CAPI_TRANSPORT_RECEIVE_FAILED       = 13
    , WSE_CAPI_TRANSPORT_MESSAGE_TOO_LARGE    = 14
    , WSE_CAPI_TRANSPORT_DATAGRAM_TRUNCATED   = 15
    , WSE_CAPI_TRANSPORT_RESOURCE_EXHAUSTED   = 16
    , WSE_CAPI_TRANSPORT_UNSUPPORTED          = 17
    , WSE_CAPI_TRANSPORT_UNKNOWN              = 18
    , WSE_CAPI_TRANSPORT_OPEN_FAILED          = 19
    , WSE_CAPI_TRANSPORT_CONFIGURATION_FAILED = 20
    , WSE_CAPI_TRANSPORT_HTTP_STATUS_ERROR    = 21
    , WSE_CAPI_TRANSPORT_RESPONSE_TOO_LARGE   = 22
    , WSE_CAPI_TRANSPORT_SECURITY_FAILED      = 23
} wse_capi_transport_error_code;

//! \~japanese `wse::xpt::eHttpMethod`と同じ値.
//! \~english  Same values as `wse::xpt::eHttpMethod`.
typedef enum wse_capi_http_method
{
      WSE_CAPI_HTTP_GET    = 0
    , WSE_CAPI_HTTP_HEAD   = 1
    , WSE_CAPI_HTTP_POST   = 2
    , WSE_CAPI_HTTP_PUT    = 3
    , WSE_CAPI_HTTP_PATCH  = 4
    , WSE_CAPI_HTTP_DELETE = 5
} wse_capi_http_method;

//!
//! \~japanese
//! @brief   全XPT Operationが要求する明示制御.
//! @details `timeout_milliseconds`は0以上でなければならない. `cancellation`は`NULL`可能で、
//!          その場合は取消を観測しない.
//! \~english
//! @brief   Explicit controls required by every XPT operation.
//! @details `timeout_milliseconds` must be non-negative. `cancellation` may be `NULL`, in which
//!          case no cancellation is observed.
//!
typedef struct wse_capi_operation_context
{
    int64_t timeout_milliseconds;
    wse_capi_cancellation cancellation;
} wse_capi_operation_context;

//! \~japanese TCP接続Handle. Move-onlyかつ呼出元Thread前提である.
//! \~english  TCP connection handle; move-only and caller-confined.
typedef struct wse_capi_tcp_client_t* wse_capi_tcp_client;

//! \~japanese UDP Socket Handle.
//! \~english  UDP socket handle.
typedef struct wse_capi_udp_client_t* wse_capi_udp_client;

//! \~japanese 受信済みUDP Datagram Handle.
//! \~english  Handle to one received UDP datagram.
typedef struct wse_capi_udp_datagram_t* wse_capi_udp_datagram;

//! \~japanese Serial Port Handle.
//! \~english  Serial port handle.
typedef struct wse_capi_serial_port_t* wse_capi_serial_port;

//! \~japanese 組立中のHTTP Request Handle.
//! \~english  Handle to an HTTP request under construction.
typedef struct wse_capi_http_request_t* wse_capi_http_request;

//! \~japanese 受信済みHTTP Response Handle.
//! \~english  Handle to a received HTTP response.
typedef struct wse_capi_http_response_t* wse_capi_http_response;

// ----------------------------------------------------------------------------------------------
// TCP
// ----------------------------------------------------------------------------------------------

//! \~japanese 未接続のTCP Clientを生成する.
//! \~english  Creates a disconnected TCP client.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_create(
    wse_capi_tcp_client* p_client_out );

//! \~japanese TCP Clientを解放する. 接続中なら閉じる. `NULL`は無視する.
//! \~english  Releases a TCP client, closing an open connection; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_tcp_client_destroy( wse_capi_tcp_client client_inout );

//! \~japanese Host／Portへ同期接続する.
//! \~english  Connects synchronously to a host and port.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_connect(
      wse_capi_tcp_client               client_inout
    , const char*                       host_in
    , uint16_t                          port_in
    , const wse_capi_operation_context* context_in );

//! \~japanese 接続を閉じる. 複数回呼出可能である.
//! \~english  Closes the connection; calling it again is safe.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_disconnect(
    wse_capi_tcp_client client_inout );

//! \~japanese Local側が接続を保持する場合に非0を返す.
//! \~english  Reports non-zero while the local connection state is held.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_is_connected(
      wse_capi_tcp_client client_in
    , wse_capi_bool*      p_connected_out );

//!
//! \~japanese
//! @brief   PeerのFIN／RSTが観測可能かを受信Dataを消費せず確認する.
//! @details 成功はEnd-to-endの生存証明ではない.
//! \~english
//! @brief   Checks for an observable peer FIN or reset without consuming pending data.
//! @details Success is not an end-to-end liveness proof.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_check_peer_connection(
    wse_capi_tcp_client client_inout );

//! \~japanese 接続中のRemote Endpointを取得する. Hostは2回呼出方式である.
//! \~english  Reads the connected remote endpoint; the host uses the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_remote_endpoint(
      wse_capi_tcp_client client_in
    , char*               p_host_buffer_out
    , size_t*             p_host_size_out
    , uint16_t*           p_port_out
    , size_t              host_capacity_in );

//! \~japanese OSが選択したLocal Endpointを取得する. Hostは2回呼出方式である.
//! \~english  Reads the OS-selected local endpoint; the host uses the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_local_endpoint(
      wse_capi_tcp_client client_in
    , char*               p_host_buffer_out
    , size_t*             p_host_size_out
    , uint16_t*           p_port_out
    , size_t              host_capacity_in );

//! \~japanese Buffer全体を同期送信し、送信済みByte数を返す.
//! \~english  Sends the complete buffer synchronously and reports the byte count sent.
//! \~japanese 非NULLのCount出力は検査前に0化し、失敗時も既知の完了数を保持する。Peer確認ではない。
//! \~english  A non-NULL count output is zeroed before validation and retains known progress on failure.
//!            It does not prove peer acceptance; zero after an exception does not prove no wire effects.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_send(
      wse_capi_tcp_client               client_inout
    , size_t*                           p_sent_out
    , const uint8_t*                    data_in
    , size_t                            size_in
    , const wse_capi_operation_context* context_in );

//!
//! \~japanese
//! @brief   最大Sizeまでの一回分を同期受信する.
//! @param [in,out] client_inout  受信元Client handle. `NULL`のときInvalidArgumentを返す.
//! @param [out] p_data_out  受信Byte列を所有するHandle. 成功時のみ設定する.
//! @param [in]  maximum_size_in  一回の受信で受け取る上限 [byte].
//! @param [in]  context_in  Timeout／Cancellationを運ぶ操作Context. `NULL`のときInvalidArgumentを返す.
//! @details Peer closeは`WSE_CAPI_TRANSPORT_REMOTE_CLOSED`として返る.
//! \~english
//! @brief   Receives one chunk up to the maximum size synchronously.
//! @param [in,out] client_inout  Client to receive from; `NULL` yields InvalidArgument.
//! @param [out] p_data_out  Handle owning the received bytes; set on success only.
//! @param [in]  maximum_size_in  Upper bound in bytes for this single receive.
//! @param [in]  context_in  Operation context carrying timeout and cancellation; `NULL` yields InvalidArgument.
//! @details A peer close is reported as `WSE_CAPI_TRANSPORT_REMOTE_CLOSED`.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_tcp_client_receive(
      wse_capi_tcp_client               client_inout
    , wse_capi_frame_buffer*            p_data_out
    , size_t                            maximum_size_in
    , const wse_capi_operation_context* context_in );

// ----------------------------------------------------------------------------------------------
// UDP
// ----------------------------------------------------------------------------------------------

//! \~japanese 1 Datagramの上限Byte数を返す.
//! \~english  Returns the maximum datagram size in bytes.
WSE_CAPI size_t WSE_CAPI_CALL wse_capi_udp_maximum_datagram_size( void );

//! \~japanese 未Bind のUDP Clientを生成する.
//! \~english  Creates an unbound UDP client.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_create(
    wse_capi_udp_client* p_client_out );

//! \~japanese UDP Clientを解放する. `NULL`は無視する.
//! \~english  Releases a UDP client; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_udp_client_destroy( wse_capi_udp_client client_inout );

//! \~japanese Local Endpointへ Bind する. Port 0はOSによる自動割当を表す.
//! \~english  Binds to a local endpoint; port zero requests an OS-assigned port.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_bind(
      wse_capi_udp_client               client_inout
    , const char*                       host_in
    , uint16_t                          port_in
    , const wse_capi_operation_context* context_in );

//! \~japanese Socketを閉じる. 複数回呼出可能である.
//! \~english  Closes the socket; calling it again is safe.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_close( wse_capi_udp_client client_inout );

//! \~japanese Socketが開いている場合に非0を返す.
//! \~english  Reports non-zero while the socket is open.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_is_open(
      wse_capi_udp_client client_in
    , wse_capi_bool*      p_open_out );

//! \~japanese Bind済みLocal Endpointを取得する. Hostは2回呼出方式である.
//! \~english  Reads the bound local endpoint; the host uses the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_local_endpoint(
      wse_capi_udp_client client_in
    , char*               p_host_buffer_out
    , size_t*             p_host_size_out
    , uint16_t*           p_port_out
    , size_t              host_capacity_in );

//! \~japanese Remote Endpointへ1 Datagramを送信する.
//! \~english  Sends one datagram to a remote endpoint.
//! \~japanese 非NULLのCount出力は検査前に0化し、失敗時も既知の完了数を保持する。Peer確認ではない。
//! \~english  A non-NULL count output is zeroed before validation and retains known progress on failure.
//!            It does not prove peer acceptance; zero after an exception does not prove no wire effects.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_send_to(
      wse_capi_udp_client               client_inout
    , size_t*                           p_sent_out
    , const char*                       host_in
    , uint16_t                          port_in
    , const uint8_t*                    data_in
    , size_t                            size_in
    , const wse_capi_operation_context* context_in );

//! \~japanese 1 Datagramを受信する. 成功時のみ`p_datagram_out`を設定する.
//! \~english  Receives one datagram, setting `p_datagram_out` on success only.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_receive_from(
      wse_capi_udp_client               client_inout
    , wse_capi_udp_datagram*            p_datagram_out
    , size_t                            maximum_size_in
    , const wse_capi_operation_context* context_in );

//!
//! \~japanese
//! @brief   1 Datagramを受信し、切詰め時も受信Prefixと送信元を返す。
//! @param [in,out] client_inout  受信Client。NULLはInvalidArgument。
//! @param [out] p_datagram_out  検査前にNULL化。成功／DatagramTruncated時は所有Handleを返し、失敗でも解放が必要。
//! @param [in] maximum_size_in  受信上限Byte数。
//! @param [in] context_in  Timeout／Cancellation。NULLはInvalidArgument。
//! @details 破棄されたSuffixは次回取得できない。確保／変換例外ではHandleなし。既存receive_fromは成功時のみ出力する。
//! \~english
//! @brief   Receives one datagram, preserving its prefix and source on truncation.
//! @param [in,out] client_inout  Receiving client; NULL yields InvalidArgument.
//! @param [out] p_datagram_out  Zeroed before validation; owned on success or DatagramTruncated. Destroy even on failure.
//! @param [in] maximum_size_in  Maximum payload bytes to retain.
//! @param [in] context_in  Timeout/cancellation; NULL yields InvalidArgument.
//! @details The discarded suffix cannot be read later. Allocation/conversion exceptions publish no handle.
//!          The existing receive_from entry point continues to publish only on success.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_udp_client_receive_from_with_progress(
      wse_capi_udp_client               client_inout
    , wse_capi_udp_datagram*            p_datagram_out
    , size_t                            maximum_size_in
    , const wse_capi_operation_context* context_in );

//! \~japanese Datagramを解放する. `NULL`は無視する.
//! \~english  Releases a datagram; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_udp_datagram_destroy( wse_capi_udp_datagram datagram_inout );

//! \~japanese 送信元Endpointを取得する. Hostは2回呼出方式である.
//! \~english  Reads the source endpoint; the host uses the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_udp_datagram_source(
      wse_capi_udp_datagram datagram_in
    , char*                 p_host_buffer_out
    , size_t*               p_host_size_out
    , uint16_t*             p_port_out
    , size_t                host_capacity_in );

//! \~japanese Payloadを複製する. 2回呼出方式である.
//! \~english  Copies the payload using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_udp_datagram_payload(
      wse_capi_udp_datagram datagram_in
    , uint8_t*              p_buffer_out
    , size_t*               p_size_out
    , size_t                capacity_in );

// ----------------------------------------------------------------------------------------------
// Serial
// ----------------------------------------------------------------------------------------------

//! \~japanese 未OpenのSerial Portを生成する.
//! \~english  Creates a closed serial port.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_create(
    wse_capi_serial_port* p_port_out );

//! \~japanese Serial Portを解放する. `NULL`は無視する.
//! \~english  Releases a serial port; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_serial_port_destroy( wse_capi_serial_port port_inout );

//! \~japanese Device名とBaud rateでOpenする.
//! \~english  Opens the port by device name and baud rate.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_open(
      wse_capi_serial_port              port_inout
    , const char*                       device_name_in
    , int32_t                           baud_rate_in
    , const wse_capi_operation_context* context_in );

//! \~japanese Portを閉じる. 複数回呼出可能である.
//! \~english  Closes the port; calling it again is safe.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_close( wse_capi_serial_port port_inout );

//! \~japanese Portが開いている場合に非0を返す.
//! \~english  Reports non-zero while the port is open.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_is_open(
      wse_capi_serial_port port_in
    , wse_capi_bool*       p_open_out );

//! \~japanese Open済みDevice名を取得する. 2回呼出方式である.
//! \~english  Reads the opened device name using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_device_name(
      wse_capi_serial_port port_in
    , char*                p_buffer_out
    , size_t*              p_size_out
    , size_t               capacity_in );

//! \~japanese 設定済みBaud rateを取得する.
//! \~english  Reads the configured baud rate.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_baud_rate(
      wse_capi_serial_port port_in
    , int32_t*             p_baud_rate_out );

//! \~japanese Buffer全体を同期送信し、送信済みByte数を返す.
//! \~english  Sends the complete buffer synchronously and reports the byte count sent.
//! \~japanese 非NULLのCount出力は検査前に0化し、失敗時も既知の完了数を保持する。Peer確認ではない。
//! \~english  A non-NULL count output is zeroed before validation and retains known progress on failure.
//!            It does not prove peer acceptance; zero after an exception does not prove no wire effects.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_send(
      wse_capi_serial_port              port_inout
    , size_t*                           p_sent_out
    , const uint8_t*                    data_in
    , size_t                            size_in
    , const wse_capi_operation_context* context_in );

//! \~japanese 最大Sizeまでの一回分を同期受信する.
//! \~english  Receives one chunk up to the maximum size synchronously.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_serial_port_receive(
      wse_capi_serial_port              port_inout
    , wse_capi_frame_buffer*            p_data_out
    , size_t                            maximum_size_in
    , const wse_capi_operation_context* context_in );

// ----------------------------------------------------------------------------------------------
// HTTP
// ----------------------------------------------------------------------------------------------

//! \~japanese Method とURLでHTTP Requestを生成する.
//! \~english  Creates an HTTP request from a method and URL.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_request_create(
      wse_capi_http_request* p_request_out
    , int32_t                method_in
    , const char*            url_in );

//! \~japanese HTTP Requestを解放する. `NULL`は無視する.
//! \~english  Releases an HTTP request; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_http_request_destroy( wse_capi_http_request request_inout );

//! \~japanese Header field を追加する.
//! \~english  Adds one header field.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_request_add_header(
      wse_capi_http_request request_inout
    , const char*           name_in
    , const char*           value_in );

//! \~japanese Request bodyを設定する. 呼出元Memoryは保持しない.
//! \~english  Sets the request body without retaining caller memory.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_request_set_body(
      wse_capi_http_request request_inout
    , const uint8_t*        body_in
    , size_t                size_in );

//!
//! \~japanese
//! @brief   HTTP Requestを実行する.
//! @param [in]  request_in                     実行するRequest handle. `NULL`のときInvalidArgumentを返す.
//! @param [out] p_response_out                 Response Handle. 成功時に設定する。加えて、Serverが
//!                                             4xx／5xxを返した失敗( WSE_CAPI_TRANSPORT_HTTP_STATUS_ERROR )でも
//!                                             設定する。それ以外の失敗では`NULL`を書く。設定された
//!                                             場合は失敗時も呼出元が解放する。
//! @param [in]  maximum_response_body_size_in  Response bodyの受入上限 [byte].
//! @param [in]  context_in                     Timeout／Cancellationを運ぶ操作Context. `NULL`のときInvalidArgumentを返す.
//! \~english
//! @brief   Executes an HTTP request.
//! @param [in]  request_in                     Request to execute; `NULL` yields InvalidArgument.
//! @param [out] p_response_out                 Response handle. Set on success, and also when the
//!                                             server answered with a 4xx or 5xx status
//!                                             ( WSE_CAPI_TRANSPORT_HTTP_STATUS_ERROR ), because that
//!                                             exchange still produced a response. Set to `NULL`
//!                                             on every other failure. When it is set the caller
//!                                             owns it, on the failure path too.
//! @param [in]  maximum_response_body_size_in  Accepted response-body ceiling in bytes.
//! @param [in]  context_in                     Operation context carrying timeout and cancellation; `NULL` yields InvalidArgument.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_execute(
      wse_capi_http_request             request_in
    , wse_capi_http_response*           p_response_out
    , size_t                            maximum_response_body_size_in
    , const wse_capi_operation_context* context_in );

//!
//! \~japanese
//! @brief   Server negotiated 認証付きでHTTP Requestを実行する.
//! @details Credentialは呼出中のみ参照し、実装側は保持しない.
//! @param [in]  request_in      実行するRequest handle. `NULL`のときInvalidArgumentを返す.
//! @param [out] p_response_out  `wse_capi_http_execute`と同じく、4xx／5xxの失敗でも設定する.
//! @param [in]  maximum_response_body_size_in  Response bodyの受入上限 [byte].
//! @param [in]  username_in     認証に用いるUser名. `NULL`のときInvalidArgumentを返す.
//! @param [in]  secret_in       認証に用いるSecret. `NULL`は空文字列として扱う.
//! @param [in]  context_in      Timeout／Cancellationを運ぶ操作Context. `NULL`のときInvalidArgumentを返す.
//! \~english
//! @brief   Executes an HTTP request with server-negotiated authentication.
//! @details Credentials are read during the call only and are never retained.
//! @param [in]  request_in      Request to execute; `NULL` yields InvalidArgument.
//! @param [out] p_response_out  Set on a 4xx or 5xx failure too, exactly as `wse_capi_http_execute`.
//! @param [in]  maximum_response_body_size_in  Accepted response-body ceiling in bytes.
//! @param [in]  username_in     User name presented for authentication; `NULL` yields InvalidArgument.
//! @param [in]  secret_in       Secret presented for authentication; `NULL` is treated as an empty secret.
//! @param [in]  context_in      Operation context carrying timeout and cancellation; `NULL` yields InvalidArgument.
//!
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_execute_authenticated(
      wse_capi_http_request             request_in
    , wse_capi_http_response*           p_response_out
    , size_t                            maximum_response_body_size_in
    , const char*                       username_in
    , const char*                       secret_in
    , const wse_capi_operation_context* context_in );

//! \~japanese HTTP Responseを解放する. `NULL`は無視する.
//! \~english  Releases an HTTP response; `NULL` is ignored.
WSE_CAPI void WSE_CAPI_CALL wse_capi_http_response_destroy( wse_capi_http_response response_inout );

//! \~japanese HTTP status codeを取得する.
//! \~english  Reads the HTTP status code.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_response_status_code(
      wse_capi_http_response response_in
    , uint16_t*              p_status_code_out );

//! \~japanese Retryを含む試行回数を取得する.
//! \~english  Reads the attempt count including retries.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_response_attempt_count(
      wse_capi_http_response response_in
    , uint32_t*              p_attempt_count_out );

//! \~japanese Response headerの個数を取得する.
//! \~english  Reads the number of response headers.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_response_header_count(
      wse_capi_http_response response_in
    , size_t*                p_count_out );

//! \~japanese 指定Index のHeader名を取得する. 2回呼出方式である.
//! \~english  Reads the header name at an index using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_response_header_name(
      wse_capi_http_response response_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 index_in
    , size_t                 capacity_in );

//! \~japanese 指定Index のHeader値を取得する. 2回呼出方式である.
//! \~english  Reads the header value at an index using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_response_header_value(
      wse_capi_http_response response_in
    , char*                  p_buffer_out
    , size_t*                p_size_out
    , size_t                 index_in
    , size_t                 capacity_in );

//! \~japanese Response bodyを複製する. 2回呼出方式である.
//! \~english  Copies the response body using the two-call pattern.
WSE_CAPI wse_capi_status WSE_CAPI_CALL wse_capi_http_response_body(
      wse_capi_http_response response_in
    , uint8_t*               p_buffer_out
    , size_t*                p_size_out
    , size_t                 capacity_in );

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WONDERSTEWENGINE_WSE_CAPI_XPT_H
