//*****************************************************************************************************************
//!
//! @file    binController.cpp
//! @brief   \~japanese WSE独自バイナリ形式の型付き数値配列入出力を実装する。
//! @brief   \~english  Implements typed numeric array I/O for the WSE binary format.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Aug-30, 2025   Create New     WapitiStew.
//!   Aug-04, 2026   Correct file documentation     WapitiStew.
//!
//! @details
//!   \~japanese 型種別、要素数、数値列の順にデータブロックを読み書きし、big-endian環境ではbyte順を変換する。
//!   \~english  Reads and writes type, element-count, and value blocks and converts byte order on big-endian systems.
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************

#include "../../../api/gef/bin/binController.h"

#include <wse/stew.h>


// C/C++
#include <algorithm>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <limits>
#include "../FileOperations.h"


#pragma warning(push)
#pragma warning(disable: 4244)  // バイナリ入出力時の固定幅数値型変換を局所的に許可する。

namespace wse
{
namespace gef
{
    static bool isBigEndian()
    {
        uint16_t test = 0x0102;
        uint8_t* ptr = reinterpret_cast<uint8_t*>(&test);
        return ptr[0] == 0x01;
    }

    static GefStatus readFailure( const std::string& message_in )
    {
        return GefStatus::failure( GefError( eGefErrorCategory::Io, eGefErrorCode::ReadFailed, message_in ) );
    }

    
    
    //--------------------------------------------------------------------------------
    // Constractor / Destructor
    //--------------------------------------------------------------------------------
    //
    // @brief  Default constructor.
    //
    BINController::BINController()
        : m_uint08_contents   ()
        , m_int08_contents    ()
        , m_int16_contents    ()
        , m_int32_contents    ()
        , m_int64_contents    ()
        , m_float32_contents  ()
        , m_float64_contents  ()
        , m_header_next_index ( 0 )
        , m_order_list        ()
    {
    }
    //
    // @brief  Destructor.
    //
    BINController::~BINController()
    {
    }
    //----------------------------------
    // Accessor
    //----------------------------------
    uint64_t  BINController::last_index ( void ) const
    {
        if( this->m_header_next_index == 0 )
        {
            throw std::logic_error( "last_index() requires at least one stored block; check index_num() first" );
        }
        return this->m_header_next_index - 1;
    }

    template< typename _Tp > std::vector< _Tp > BINController::contents( const size_t index_in )const
    {
        std::vector< _Tp >value;
        bool is_detected = false;
        if( !this->m_uint08_contents  .empty() )
        {
            for( const std::pair< size_t, uint8_t    >& val : this->m_uint08_contents )
            {
                if( val.first == index_in )
                {
                    is_detected = true;
                    value.emplace_back( static_cast< _Tp >( val.second ) );
                }
                else if( val.first != index_in && is_detected ){ break; }
            }
        }
        if( is_detected ){ return value; }
        if( !this->m_int08_contents  .empty() )
        {
            for( const std::pair< size_t, int8_t    >& val : this->m_int08_contents )
            {
                if( val.first == index_in )
                {
                    is_detected = true;
                    value.emplace_back( static_cast< _Tp >( val.second ) );
                }
                else if( val.first != index_in && is_detected ){ break; }
            }
        }
        if( is_detected ){ return value; }
        if( !this->m_int16_contents  .empty() )
        {
            for( const std::pair< size_t, int16_t    >& val : this->m_int16_contents )
            {
                if( val.first == index_in )
                {
                    is_detected = true;
                    value.emplace_back( static_cast< _Tp >( val.second ) );
                }
                else if( val.first != index_in && is_detected ){ break; }
            }
        }
        if( is_detected ){ return value; }
        if( !this->m_int32_contents  .empty() )
        {
            for( const std::pair< size_t, int32_t    >& val : this->m_int32_contents )
            {
                if( val.first == index_in )
                {
                    is_detected = true;
                    value.emplace_back( static_cast< _Tp >( val.second ) );
                }
                else if( val.first != index_in && is_detected ){ break; }
            }
        }
        if( is_detected ){ return value; }
        if( !this->m_int64_contents  .empty() )
        {
            for( const std::pair< size_t, int64_t    >& val : this->m_int64_contents )
            {
                if( val.first == index_in )
                {
                    is_detected = true;
                    value.emplace_back( static_cast< _Tp >( val.second ) );
                }
                else if( val.first != index_in && is_detected ){ break; }
            }
        }
        if( is_detected ){ return value; }
        if( !this->m_float32_contents.empty() )
        {
            for( const std::pair< size_t, float32_t    >& val : this->m_float32_contents )
            {
                if( val.first == index_in )
                {
                    is_detected = true;
                    value.emplace_back( static_cast< _Tp >( val.second ) );
                }
                else if( val.first != index_in && is_detected ){ break; }
            }
        }
        if( is_detected ){ return value; }
        if( !this->m_float64_contents.empty() )
        {
            for( const std::pair< size_t, float64_t    >& val : this->m_float64_contents )
            {
                if( val.first == index_in )
                {
                    is_detected = true;
                    value.emplace_back( static_cast< _Tp >( val.second ) );
                }
                else if( val.first != index_in && is_detected ){ break; }
            }
        }
        return value;
    }
    template WSE_API std::vector< uint8_t   > BINController::contents< uint8_t   >( const size_t index )const;
    template WSE_API std::vector< int8_t    > BINController::contents< int8_t    >( const size_t index )const;
    template WSE_API std::vector< int16_t   > BINController::contents< int16_t   >( const size_t index )const;
    template WSE_API std::vector< int32_t   > BINController::contents< int32_t   >( const size_t index )const;
    template WSE_API std::vector< int64_t   > BINController::contents< int64_t   >( const size_t index )const;
    template WSE_API std::vector< float32_t > BINController::contents< float32_t >( const size_t index )const;
    template WSE_API std::vector< float64_t > BINController::contents< float64_t >( const size_t index )const;


    template<class T>
    static void reserveAppend(std::vector<T>& values_inout, const size_t count_in)
    {
        const auto needed = values_inout.size() + count_in;
        if (needed <= values_inout.capacity()) return;
        const auto capacity = values_inout.capacity();
        const auto growth = (std::max)(capacity, size_t{1});
        const auto grown = growth > values_inout.max_size() - capacity
            ? values_inout.max_size() : capacity + growth;
        values_inout.reserve((std::max)(needed, grown));
    }

    template<class T>
    static void appendBlock(std::vector<std::pair<size_t, T>>& values_inout,
        std::vector<std::pair<eCategory, size_t>>& order_inout, uint64_t& index_inout,
        const std::vector<T>& values_in, const eCategory category_in)
    {
        if (index_inout == (std::numeric_limits<size_t>::max)() ||
            values_in.size() > values_inout.max_size() - values_inout.size() ||
            order_inout.size() == order_inout.max_size())
            throw std::length_error("the BIN block store is full");
        // Reserve both owners before publishing any value, order entry or index.
        reserveAppend(values_inout, values_in.size());
        reserveAppend(order_inout, 1);
        for (const auto value : values_in) values_inout.emplace_back(static_cast<size_t>(index_inout), value);
        order_inout.emplace_back(category_in, values_in.size());
        ++index_inout;
    }

    void BINController::setContents(const std::vector<uint8_t>& values_in)
    { appendBlock(m_uint08_contents, m_order_list, m_header_next_index, values_in, eCategory::Enum); }
    void BINController::setContents(const std::vector<int8_t>& values_in)
    { appendBlock(m_int08_contents, m_order_list, m_header_next_index, values_in, eCategory::Integer08); }
    void BINController::setContents(const std::vector<int16_t>& values_in)
    { appendBlock(m_int16_contents, m_order_list, m_header_next_index, values_in, eCategory::Integer16); }
    void BINController::setContents(const std::vector<int32_t>& values_in)
    { appendBlock(m_int32_contents, m_order_list, m_header_next_index, values_in, eCategory::Integer32); }
    void BINController::setContents(const std::vector<int64_t>& values_in)
    { appendBlock(m_int64_contents, m_order_list, m_header_next_index, values_in, eCategory::Integer64); }
    void BINController::setContents(const std::vector<float32_t>& values_in)
    { appendBlock(m_float32_contents, m_order_list, m_header_next_index, values_in, eCategory::Float32); }
    void BINController::setContents(const std::vector<float64_t>& values_in)
    { appendBlock(m_float64_contents, m_order_list, m_header_next_index, values_in, eCategory::Float64); }

    //--------------------------------------------------------------------------------
    // Specific Method
    //------------------------------------------------------------------------------

    // 
    // @brief  Read.
    //
    // @param [in ] path_in
    // 
    GefStatus BINController::read(const std::string& path_in)
    {
        return readWithLimits(path_in, ReadLimits::unlimited());
    }

    GefStatus BINController::readWithLimits(const std::string& path_in, const ReadLimits& limits_in)
    {
        BINController staged;
        std::ifstream file(path_in, std::ios::binary);

        if (!file.is_open()) {
            return GefStatus::failure( GefError(
                eGefErrorCategory::Io, eGefErrorCode::FileOpenFailed, "the binary file cannot be opened" ) );
        }

        bool is_bigendian = isBigEndian();
        size_t header_id = 0;
        uint64_t input_bytes = 0;
        uint64_t elements = 0;
        while( file.peek() != EOF )
        {
            if (header_id >= limits_in.max_blocks ||
                header_id == (std::numeric_limits<size_t>::max)() ||
                limits_in.max_input_bytes - input_bytes < 9)
                return detail::limitFailure();
            uint8_t type = 0;
            if( !file.read( reinterpret_cast< char* >( &type ), sizeof( uint8_t ) ) )
            {
                return readFailure( "the binary block type cannot be read" );
            }
            eCategory type_category = static_cast< eCategory >( type );

            uint64_t num = 0;
            if( !file.read( reinterpret_cast< char* >( &num ), sizeof( uint64_t ) ) )
            {
                return readFailure( "the binary block element count cannot be read" );
            }

            if (is_bigendian)
            {
                auto* bytes = reinterpret_cast<uint8_t*>(&num);
                std::reverse(bytes, bytes + sizeof(num));
            }
            static constexpr uint64_t widths[] = {0, 1, 1, 2, 4, 8, 4, 8};
            if (type == 0 || type >= sizeof(widths) / sizeof(widths[0]))
                return GefStatus::failure(GefError(eGefErrorCategory::Format,
                    eGefErrorCode::MalformedData, "the binary block category is unknown"));
            input_bytes += 9;
            const uint64_t width = widths[type];
            if (num > limits_in.max_elements - elements ||
                num > (limits_in.max_input_bytes - input_bytes) / width ||
                num > (std::numeric_limits<size_t>::max)())
                return detail::limitFailure();
            input_bytes += num * width;
            elements += num;

            switch( type_category )
            {
                case eCategory::Enum :
                {
                    for( uint64_t i = 0; i < num; ++i )
                    {
                        uint8_t val = 0;
                        if( !file.read( reinterpret_cast< char* >( &val ), sizeof( uint8_t ) ) )
                        {
                            return readFailure( "a binary block value cannot be read" );
                        }
                        staged.m_uint08_contents.emplace_back( std::pair< size_t, uint8_t >( header_id, val ) );
                    }
                }break;
                case eCategory::Integer08 :
                {
                    for( uint64_t i = 0; i < num; ++i )
                    {
                        int8_t val = 0;
                        if( !file.read( reinterpret_cast< char* >( &val ), sizeof( int8_t ) ) )
                        {
                            return readFailure( "a binary block value cannot be read" );
                        }
                        staged.m_int08_contents.emplace_back( std::pair< size_t, int8_t >( header_id, val ) );
                    }
                }break;
                case eCategory::Integer16 :
                {
                    for( uint64_t i = 0; i < num; ++i )
                    {
                        uint8_t bytes[ sizeof( int16_t ) ];
                        if( !file.read( reinterpret_cast< char* >( &bytes ), sizeof( int16_t ) ) )
                        {
                            return readFailure( "a binary block value cannot be read" );
                        }
                        if( is_bigendian ){ std::reverse( bytes, bytes + sizeof( int16_t ) ); }
                        int16_t value;
                        std::memcpy(&value, bytes, sizeof(int16_t));
                        staged.m_int16_contents.emplace_back( std::pair< size_t, int16_t >( header_id, value ) );
                    }
                }break;

                case eCategory::Integer32 :
                {
                    for( uint64_t i = 0; i < num; ++i )
                    {
                        uint8_t bytes[ sizeof( int32_t ) ];
                        if( !file.read( reinterpret_cast< char* >( &bytes ), sizeof( int32_t ) ) )
                        {
                            return readFailure( "a binary block value cannot be read" );
                        }
                        if( is_bigendian ){ std::reverse( bytes, bytes + sizeof( int32_t ) ); }
                        int32_t value;
                        std::memcpy(&value, bytes, sizeof(int32_t));
                        staged.m_int32_contents.emplace_back( std::pair< size_t, int32_t >( header_id, value ) );
                    }
                }break;

                case eCategory::Integer64 :
                {
                    for( uint64_t i = 0; i < num; ++i )
                    {
                        uint8_t bytes[ sizeof( int64_t ) ];
                        if( !file.read( reinterpret_cast< char* >( &bytes ), sizeof( int64_t ) ) )
                        {
                            return readFailure( "a binary block value cannot be read" );
                        }
                        if( is_bigendian ){ std::reverse( bytes, bytes + sizeof( int64_t ) ); }
                        int64_t value;
                        std::memcpy(&value, bytes, sizeof(int64_t));
                        staged.m_int64_contents.emplace_back( std::pair< size_t, int64_t >( header_id, value ) );
                    }
                }break;

                case eCategory::Float32 :
                {
                    for( uint64_t i = 0; i < num; ++i )
                    {
                        uint8_t bytes[ sizeof( float32_t ) ];
                        if( !file.read( reinterpret_cast< char* >( &bytes ), sizeof( float32_t ) ) )
                        {
                            return readFailure( "a binary block value cannot be read" );
                        }
                        if( is_bigendian ){ std::reverse( bytes, bytes + sizeof( float32_t ) ); }
                        float32_t value;
                        std::memcpy(&value, bytes, sizeof(float32_t));
                        staged.m_float32_contents.emplace_back( std::pair< size_t, float32_t >( header_id, value ) );
                    }
                }break;
                case eCategory::Float64 :
                {
                    for( uint64_t i = 0; i < num; ++i )
                    {
                        uint8_t bytes[ sizeof( float64_t ) ];
                        if( !file.read( reinterpret_cast< char* >( &bytes ), sizeof( float64_t ) ) )
                        {
                            return readFailure( "a binary block value cannot be read" );
                        }
                        if( is_bigendian ){ std::reverse( bytes, bytes + sizeof( float64_t ) ); }
                        float64_t value;
                        std::memcpy(&value, bytes, sizeof(float64_t));
                        staged.m_float64_contents.emplace_back( std::pair< size_t, float64_t >( header_id, value ) );
                    }
                }break;
                case eCategory::None:
                default:
                {
                    return GefStatus::failure( GefError(
                        eGefErrorCategory::Format, eGefErrorCode::MalformedData,
                        "the binary block category is unknown" ) );
                }
            }
            staged.m_order_list.emplace_back(type_category, static_cast<size_t>(num));
            ++header_id;
        }

        if (file.bad() || !file.eof()) return readFailure("the binary stream failed");
        file.clear(); // Expected EOF is not a close failure.
        file.close();
        if (file.fail()) return readFailure("the binary stream cannot be closed");
        staged.m_header_next_index = header_id;
        swap(*this, staged);

        return GefStatus::success();
    }

    // 
    // @brief  Read.
    //
    // @param [in ] path_in
    // 
    GefStatus BINController::write ( const std::string& path_in, bool is_update_in )
    {

        std::ios_base::openmode mode = std::ios::binary;
        if (is_update_in) 
        {
            mode |= std::ios::app;
        } 
        else 
        {
            mode |= std::ios::trunc;
        }

        std::ofstream file(path_in, mode);

        if (!file.is_open()) 
        {
            return GefStatus::failure( GefError(
                eGefErrorCategory::Io, eGefErrorCode::FileOpenFailed, "the binary file cannot be opened" ) );
        }

        size_t next_index__uint08 = 0;
        size_t next_index___int08 = 0;
        size_t next_index___int16 = 0;
        size_t next_index___int32 = 0;
        size_t next_index___int64 = 0;
        size_t next_index_float32 = 0;
        size_t next_index_float64 = 0;
        bool is_bigendian = isBigEndian();

        for( const std::pair< eCategory, size_t >& order : this->m_order_list )
        {
            // 型を書き込む.
            {
                const eCategory category = order.first;
                const uint8_t byte = static_cast< uint8_t >( category );
                file.write( reinterpret_cast< const char* >( &byte ), sizeof( uint8_t ) ); 
            }

            // 数を書き込む.
            {
                uint64_t size = order.second;
                uint8_t byte[sizeof(uint64_t)];
                std::memcpy(byte, &size, sizeof(uint64_t));
                if( is_bigendian ){ std::reverse(byte, byte + sizeof(uint64_t)); }
                file.write( reinterpret_cast< char* >( &byte ), sizeof( uint64_t ) );
            }

            switch( order.first )
            {
                case eCategory::Enum     :
                { 
                    for( size_t i = next_index__uint08; i < order.second + next_index__uint08; ++i )
                    { 
                        uint8_t   val = this->m_uint08_contents [i].second;  
                        uint8_t byte[sizeof( uint8_t   )]; 
                        std::memcpy( byte, &val, sizeof( uint8_t   )); 
                        if( is_bigendian ){ std::reverse(byte, byte + sizeof( uint8_t   ));}
                        file.write( reinterpret_cast< char* >( &byte ), sizeof( uint8_t   ) ); 
                    } 
                    next_index__uint08 += order.second; 
                }break;
                case eCategory::Integer08:
                { 
                    for( size_t i = next_index___int08; i < order.second + next_index___int08; ++i )
                    { 
                        int8_t    val = this->m_int08_contents  [i].second;  
                        uint8_t byte[sizeof( int8_t    )]; 
                        std::memcpy( byte, &val, sizeof( int8_t    )); 
                        if( is_bigendian ){ std::reverse(byte, byte + sizeof( int8_t    ));}
                        file.write( reinterpret_cast< char* >( &byte ), sizeof( int8_t    ) ); 
                    } 
                    next_index___int08 += order.second; 
                }break;
                case eCategory::Integer16:
                { 
                    for( size_t i = next_index___int16; i < order.second + next_index___int16; ++i )
                    { 
                        int16_t   val = this->m_int16_contents  [i].second;  
                        uint8_t byte[sizeof( int16_t   )]; 
                        std::memcpy( byte, &val, sizeof( int16_t   )); 
                        if( is_bigendian ){ std::reverse(byte, byte + sizeof( int16_t   ));}
                        file.write( reinterpret_cast< char* >( &byte ), sizeof( int16_t   ) ); 
                    } 
                    next_index___int16 += order.second; 
                }break;
                case eCategory::Integer32:
                { 
                    for( size_t i = next_index___int32; i < order.second + next_index___int32; ++i )
                    { 
                        int32_t   val = this->m_int32_contents  [i].second;  
                        uint8_t byte[sizeof( int32_t   )]; 
                        std::memcpy( byte, &val, sizeof( int32_t   )); 
                        if( is_bigendian ){ std::reverse(byte, byte + sizeof( int32_t   ));}
                        file.write( reinterpret_cast< char* >( &byte ), sizeof( int32_t   ) ); 
                    } 
                    next_index___int32 += order.second; 
                }break;
                case eCategory::Integer64:
                { 
                    for( size_t i = next_index___int64; i < order.second + next_index___int64; ++i )
                    { 
                        int64_t   val = this->m_int64_contents  [i].second;  
                        uint8_t byte[sizeof( int64_t   )]; 
                        std::memcpy( byte, &val, sizeof( int64_t   )); 
                        if( is_bigendian ){ std::reverse(byte, byte + sizeof( int64_t   ));}
                        file.write( reinterpret_cast< char* >( &byte ), sizeof( int64_t   ) ); 
                    } 
                    next_index___int64 += order.second; 
                }break;
                case eCategory::Float32  :
                { 
                    for( size_t i = next_index_float32; i < order.second + next_index_float32; ++i )
                    { 
                        float32_t val = this->m_float32_contents[i].second;  
                        uint8_t byte[sizeof( float32_t )]; 
                        std::memcpy( byte, &val, sizeof( float32_t )); 
                        if( is_bigendian ){ std::reverse(byte, byte + sizeof( float32_t ));}
                        file.write( reinterpret_cast< char* >( &byte ), sizeof( float32_t ) ); 
                    } 
                    next_index_float32 += order.second; 
                }break;
                case eCategory::Float64  :
                { 
                    for( size_t i = next_index_float64; i < order.second + next_index_float64; ++i )
                    { 
                        float64_t val = this->m_float64_contents[i].second;  
                        uint8_t byte[sizeof( float64_t )]; 
                        std::memcpy( byte, &val, sizeof( float64_t )); 
                        if( is_bigendian ){ std::reverse(byte, byte + sizeof( float64_t ));}
                        file.write( reinterpret_cast< char* >( &byte ), sizeof( float64_t ) ); 
                    } 
                    next_index_float64 += order.second; 
                }break;
                case eCategory::None:
                default:
                {
                    throw std::logic_error( "the stored block order list holds an unknown category" );
                }
            }
        }
        return detail::finishWrite(file);
    }
   

    GefStatus BINController::writeAtomic(const std::string& path_in)
    {
        return detail::atomicWrite(path_in, [this](const std::string& temporary_in) {
            return write(temporary_in);
        });
    }

};
};


#pragma warning(pop)
