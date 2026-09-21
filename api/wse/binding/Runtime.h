//*****************************************************************************************************************
//!
//! @file    Runtime.h
//! @brief   \~japanese 多言語Bindingの共通Native facadeを定義する.
//! @brief   \~english  Defines the common native facade for language bindings.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_BINDING_RUNTIME_H
#define WONDERSTEWENGINE_WSE_BINDING_RUNTIME_H

#include "Cancellation.h"
#include "Error.h"
#include "FrameBuffer.h"
#include "../../dynamic.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace wse
{
namespace binding
{

//! \~japanese RuntimeとBinding ABIのPortable情報.
//! \~english  Portable runtime and binding-ABI information.
struct WSE_API sRuntimeInfo final
{
    std::string version;
    std::uint32_t binding_abi_version;
    bool has_xpt;
    bool has_tmr;
    bool has_oui;
    bool has_gef;
    bool has_iui;
    bool has_vpj;
};

//! \~japanese 各Language adapterが共有する同期Native処理.
//! \~english  Synchronous native operations shared by all language adapters.
class WSE_API Runtime final
{
  public:
    static constexpr std::uint32_t BINDING_ABI_VERSION = 1U;

    sRuntimeInfo info() const;

    //! \~japanese Runtime所有のByte列へCopyする. Runtime側は呼出元Memoryを保持しない.
    //! \~english  Copies bytes into runtime-owned storage without retaining caller memory.
    Result<FrameBuffer> copyFrame( const std::vector<std::uint8_t>& bytes_in ) const;

    //! \~japanese Binding worker上で使う有限待機. Cancellationを協調的に観測する.
    //! \~english  Finite wait for binding workers with cooperative cancellation.
    Status wait(
          std::chrono::milliseconds duration_in
        , const CancellationToken& cancellation_in = CancellationToken()
    ) const;
};

} // namespace binding
} // namespace wse

#endif // WONDERSTEWENGINE_WSE_BINDING_RUNTIME_H
