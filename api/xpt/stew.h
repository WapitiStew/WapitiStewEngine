//*****************************************************************************************************************
//!
//! @file    stew.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Aug-11, 2026   Create New     WapitiStew.
//!
//! @brief
//!     \~japanese 通信モジュールの共通インクルードヘッダ.
//!     \~english  Common include header for the communication module.
//!
//! @details
//!     \~japanese
//!     @n 本ファイルは、通信モジュールの公開APIを一括でインクルードする。
//!     @n 外部モジュールは個別ヘッダーではなく、本ヘッダーを公開入口として使用する。
//!
//!     \~english
//!     @n This file provides a unified include header for the public communication APIs.
//!     @n External modules use this header as the public entry point instead of individual headers.
//!
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#ifndef WONDERSTEWENGINE_XPT_HEADER_H
#define WONDERSTEWENGINE_XPT_HEADER_H

// define
#define IS_ENABLE_XPT_WSE

// portable operation and transport contracts.
#include "error/TransportError.h"
#include "http/HttpClient.h"
#include "network/Endpoint.h"
#include "operation/Cancellation.h"
#include "operation/OperationContext.h"
#include "retry/RetryPolicy.h"
#include "serial/SerialPort.h"
#include "tcp/TcpClient.h"
#include "udp/UdpClient.h"

#endif // WONDERSTEWENGINE_XPT_HEADER_H
