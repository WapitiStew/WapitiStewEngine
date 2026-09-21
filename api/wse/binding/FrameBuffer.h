//*****************************************************************************************************************
//!
//! @file    FrameBuffer.h
//! @brief   \~japanese Binding境界で所有権が明確なByte bufferを定義する.
//! @brief   \~english  Defines an owned byte buffer with explicit binding-boundary ownership.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#ifndef WONDERSTEWENGINE_WSE_BINDING_FRAMEBUFFER_H
#define WONDERSTEWENGINE_WSE_BINDING_FRAMEBUFFER_H

#include "../../dynamic.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace wse
{
namespace binding
{

//! \~japanese Language RuntimeへHandleを漏らさず共有できる不変Byte列.
//! \~english  Immutable bytes that can be shared without exposing native handles.
class WSE_API FrameBuffer final
{
  private:
    std::shared_ptr<const std::vector<std::uint8_t>> m_bytes;

  public:
    FrameBuffer();
    explicit FrameBuffer( const std::vector<std::uint8_t>& bytes_in );
    explicit FrameBuffer( std::vector<std::uint8_t>&& bytes_in );

    std::size_t size() const noexcept;
    bool empty() const noexcept;
    const std::vector<std::uint8_t>& bytes() const noexcept;
};

} // namespace binding
} // namespace wse

#endif // WONDERSTEWENGINE_WSE_BINDING_FRAMEBUFFER_H
