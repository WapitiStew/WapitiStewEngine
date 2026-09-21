//*****************************************************************************************************************
//!
//! @file    FrameBuffer.cpp
//! @brief   \~japanese 所有権を持つBinding Frame bufferの実装.
//! @brief   \~english  Implements the owned binding frame buffer.
//!
//! @date
//!   Aug-28, 2026   Create New.
//*****************************************************************************************************************

#include <wse/binding/FrameBuffer.h>

#include <utility>

namespace wse
{
namespace binding
{

FrameBuffer::FrameBuffer()
    : m_bytes ( std::make_shared<const std::vector<std::uint8_t>>() )
{
}

FrameBuffer::FrameBuffer( const std::vector<std::uint8_t>& bytes_in )
    : m_bytes ( std::make_shared<const std::vector<std::uint8_t>>( bytes_in ) )
{
}

FrameBuffer::FrameBuffer( std::vector<std::uint8_t>&& bytes_in )
    : m_bytes ( std::make_shared<const std::vector<std::uint8_t>>( std::move( bytes_in ) ) )
{
}

std::size_t FrameBuffer::size() const noexcept
{
    return this->m_bytes->size();
}

bool FrameBuffer::empty() const noexcept
{
    return this->m_bytes->empty();
}

const std::vector<std::uint8_t>& FrameBuffer::bytes() const noexcept
{
    return *this->m_bytes;
}

} // namespace binding
} // namespace wse
