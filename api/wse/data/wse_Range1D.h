//*****************************************************************************************************************
//! 
//! @file    wse_Range1D.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese 1次元の範囲（最小・最大）値を保持・操作する Range1_ テンプレート構造体の定義。
//!     \~english  Definition of the Range1_ template struct for holding and manipulating 1D minimum/maximum values.
//!
//!
//! @details
//!     \~japanese
//!         このヘッダファイルでは、1次元の範囲（最小値・最大値）を表す Range1_ 構造体を定義している。
//!         テンプレートにより任意のスカラー型に対応し、整数・浮動小数点両方で利用可能。
//!
//!     \~english
//!         This header defines the Range1_ struct, which represents a 1D range with minimum and maximum values.
//!         It provides functionalities such as range containment checking, length computation,
//!         Through templates, it supports various scalar types, both integral and floating-point.
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
#pragma once

#ifndef WONDERSTEWENGINE_DATA_RANGE1D_H
#define WONDERSTEWENGINE_DATA_RANGE1D_H

// include files
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#endif
#include <stdexcept>
#include "../../dynamic.h"
#include "../depend/wse_STD.h"
#include "../depend/wse_Enum.h"
#include "../depend/wse_Typedef.h"
#include "../utility/wse_StringTool.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif




namespace wse
{
    //!
    //! @struct Range1_
    //!
    //! @brief
    //!     \~japanese 1次元の範囲（最小値・最大値）を管理する汎用テンプレート構造体。
    //!     \~english  Generic template struct for managing 1D range (minimum and maximum values).
    //!
    //! @details
    //!     \~japanese
    //!         最小値と最大値の2つのスカラー値を保持し、それらを基に範囲内判定、長さ計算、文字列変換などを提供する。
    //!         任意のスカラー型に対応しており、int型やfloat型などと組み合わせて使うことができる。
        //!
    //!     \~english
    //!         Holds two scalar values representing the minimum and maximum of a 1D range,
    //!         and provides utility functions such as range containment checks, length calculation, and string conversion.
        //!
    //! @tparam _Tp
    //!     \~japanese 範囲を構成するスカラー型（例: int, float など）
    //!     \~english  Scalar type representing the range (e.g., int, float)
    //!
    //! @note
    //!     \~japanese 不正な範囲（min > max）が与えられた場合、例外がスローされる。
    //!     \~english  If an invalid range (min > max) is specified, an exception will be thrown.
    //! 
    template<typename _Tp>
    struct WSE_API Range1_      final
    {

        //!
        //! @typedef value_type
        //!
        //! @brief
        //!     \~japanese 範囲の要素型（テンプレート引数として指定された型）
        //!     \~english  Type representing the range values (as given by template argument)
        //!
        typedef _Tp value_type;
        //----------------------------------
        // Member
        //----------------------------------
        private: _Tp m_minimum; //!< \~japanese 最小値を表すスカラー値。        \~english Scalar value representing the minimum.
        private: _Tp m_maximum; //!< \~japanese 最大値を表すスカラー値。        \~english Scalar value representing the maximum.


        //----------------------------------
        // Private Menber Method
        //----------------------------------
        //!
        //! @brief
        //!     \~japanese 範囲の整合性チェック（min <= max であることを確認）
        //!     \~english  Validates internal range (ensures min <= max).
        //! 
        //! @note
        //!     \~japanese 不正な範囲が設定された場合は std::invalid_argument を送出する。
        //!     \~english  Throws std::invalid_argument if an invalid range is specified.
        //! 
        private: void checkMemberSetting( void )
        {
            if( this->m_minimum > this->m_maximum )
            {
                throw std::invalid_argument( "the range minimum must not exceed the maximum" );
            }
        }


        //----------------------------------
        // Getter
        //----------------------------------

        //!
        //! @brief
        //!     \~japanese 格納されているデータサイズ（バイト）を取得。
        //!     \~english  Returns the memory size (in bytes) of the stored data.
        //! @return size in bytes
        //! 
        public: uint64_t byte( void ) const { return sizeof( value_type ) * 2; }

        //! 
        //! @brief
        //!     \~japanese 最小値を取得。
        //!     \~english  Returns the minimum value.
        //! @return minimum value
        //! 
        public: _Tp minimum( void ) const { return this->m_minimum; }

        //! 
        //! @brief
        //!     \~japanese 最大値を取得。
        //!     \~english  Returns the maximum value.
        //! @return maximum value
        //! 
        public: _Tp maximum( void ) const { return this->m_maximum; }

        //----------------------------------
        // Setter
        //----------------------------------

        //! 
        //! @brief
        //!     \~japanese 最小値と最大値を個別に指定して設定。
        //!     \~english  Sets the minimum and maximum values explicitly.
        //! @param [in] in_min_in  Minimum value
        //! @param [in] in_max_in  Maximum value
        //! 
        public: void setup( const _Tp in_min_in, const _Tp in_max_in )
        {
            this->m_minimum = in_min_in;
            this->m_maximum = in_max_in;
            this->checkMemberSetting();
        }

        //! 
        //! @brief
        //!     \~japanese 他の Range1_ オブジェクトから設定。
        //!     \~english  Sets range values from another Range1_ object.
        //! @param [in] range_in  Source range object
        //! 
        public: void setup( const Range1_< _Tp > range_in )
        {
            this->m_minimum = range_in.minimum();
            this->m_maximum = range_in.maximum();
            this->checkMemberSetting();
        }


        //----------------------------------
        // Constractor / Destoractor
        //----------------------------------

        //!
        //! @brief Default constructor.
        //!
        public: explicit Range1_( void ) 
            : m_minimum ( static_cast< _Tp >( 0 ) )
            , m_maximum ( static_cast< _Tp >( 0 ) )
        {
        }
        //!
        //! @brief Destructor.
        //!
        public: ~Range1_( void ) {}

        //!
        //! @brief Constructor with min/max values.
        //! @param start_in Minimum value.
        //! @param end_in   Maximum value.
        //!
        public: Range1_( const _Tp start_in, const _Tp end_in )
            : m_minimum ( start_in )
            , m_maximum ( end_in )
        {
            this->checkMemberSetting();
        }

        //!
        //! @brief Constructor with min/max values of a different type.
        //! @param start_in Minimum value.
        //! @param end_in   Maximum value.
        //!
        public: template< typename _inTp > explicit 
        Range1_( const _inTp start_in, const _inTp end_in )
            : m_minimum ( static_cast<_Tp>(start_in) )
            , m_maximum ( static_cast<_Tp>(end_in) )
        {
            this->checkMemberSetting();
        }

        //!
        //! @brief Casting constructor.
        //! @param obj_in Input Range1_ object of a different type.
        //!
        public: template< typename _inTp > explicit 
        Range1_( const Range1_<_inTp>& obj_in )
            : m_minimum ( static_cast<_Tp>(obj_in.m_minimum) )
            , m_maximum ( static_cast<_Tp>(obj_in.m_maximum) )
        {
            this->checkMemberSetting();
        }

        //!
        //! @brief Copy constructor.
        //! @param obj_in Source object to copy.
        //!
        public: Range1_( const Range1_& obj_in )
            : m_minimum ( obj_in.m_minimum )
            , m_maximum ( obj_in.m_maximum )
        {
        }
        //!
        //! @brief Move constructor.
        //! @param obj_inout Rvalue reference to move.
        //!
        public:
        Range1_( Range1_&& obj_inout ) noexcept
            : m_minimum ( std::move(obj_inout.m_minimum) )
            , m_maximum ( std::move(obj_inout.m_maximum) )
        {
        }
        //!
        //! @brief Swap function.
        //! @param obj1_inout First object.
        //! @param obj2_inout Second object.
        //!
        friend void swap( Range1_<_Tp>& obj1_inout, Range1_<_Tp>& obj2_inout )
        {
            std::swap(obj1_inout.m_minimum, obj2_inout.m_minimum);
            std::swap(obj1_inout.m_maximum, obj2_inout.m_maximum);
        }

        //!
        //! @brief Copy assignment operator.
        //! @param obj Source object to copy.
        //! @return Reference to this object.
        //!
        public:
        Range1_< _Tp >& operator = ( const Range1_< _Tp >& obj )
        {
            Range1_< _Tp > copy_obj(obj);
            swap(*this, copy_obj);
            return *this;
        }

        //!
        //! @brief Move assignment operator.
        //! @param obj Source object to move.
        //! @return Reference to this object.
        //!
        public:
        Range1_< _Tp >& operator = ( Range1_<_Tp>&& obj ) noexcept
        {
            Range1_< _Tp > move_obj(std::move(obj));
            swap(*this, move_obj);
            return *this;
        }

        //!
        //! @brief Equality operator.
        //! @param data Object to compare with.
        //! @return True if equal, false otherwise.
        //!
        public:
        bool operator == ( const Range1_< _Tp >& data )
        {
            return ( this->m_minimum == data.m_minimum ) && ( this->m_maximum == data.m_maximum );
        }

        //!
        //! @brief Inequality operator.
        //! @param data Object to compare with.
        //! @return True if not equal, false otherwise.
        //!
        public:
        bool operator != ( const Range1_< _Tp >& data )
        {
            return !( *this == data );
        }

        //--------------------------------------------------------------------------------
        // Specific Method
        //--------------------------------------------------------------------------------
        
        //!
        //! @brief Check if the value is within the range.
        //! @param value_in Value to be compared.
        //! @return True if within range [minimum, maximum], false otherwise.
        //!
        public:
        bool isInner ( const _Tp& value_in ) const { return ( ( m_minimum <= value_in  ) && ( value_in <= m_maximum ) ); }

        //!
        //! @brief Check if the value is outside the range.
        //! @param value_in Value to be compared.
        //! @return True if outside range, false otherwise.
        //!
        public:
        bool isOver ( const _Tp value_in )  const { return !isInner( value_in ); }

        //!
        //! @brief Get the length of the range.
        //! @return Range length = maximum - minimum.
        //!
        public:
        _Tp length ( void )  const 
        { 
            return this->maximum() - this->minimum();
        }



        //!
        //! @brief Generate string representation of the range.
        //! @return Human-readable string representing the range values.
        //!
        public: std::string str( void ) const
        {
            const std::string os = "[ " + toString( this->minimum() ) + " <-> " + toString( this->maximum() ) + " ]";
            return os;
        }

    };


    //!
    //! @brief Output stream operator for Range1_.
    //! @tparam _Tp Scalar type.
    //! @param os Output stream.
    //! @param obj Range1_ object to output.
    //! @return Reference to the output stream.
    //!
    template< typename _Tp >
    inline static std::ostream& operator << ( std::ostream& os, const Range1_< _Tp >& obj )
    {
        os << obj.str();
        return os;
    }



    typedef Range1_<  S8 > S8_RANGE;
    typedef Range1_<  U8 > U8_RANGE;
    typedef Range1_< S16 >S16_RANGE;
    typedef Range1_< U16 >U16_RANGE;
    typedef Range1_< S32 >S32_RANGE;
    typedef Range1_< U32 >U32_RANGE;
    typedef Range1_< S64 >S64_RANGE;
    typedef Range1_< U64 >U64_RANGE;
    typedef Range1_< F32 >F32_RANGE;
    typedef Range1_< F64 >F64_RANGE;

    typedef Range1_<  S8 >  sint08_range;
    typedef Range1_< S16 >  sint16_range;
    typedef Range1_< S32 >  sint32_range;
    typedef Range1_< S64 >  sint64_range;
    typedef Range1_<  U8 >  uint08_range;
    typedef Range1_< U16 >  uint16_range;
    typedef Range1_< U32 >  uint32_range;
    typedef Range1_< U64 >  uint64_range;
    typedef Range1_< F32 > float32_range;
    typedef Range1_< F64 > float64_range;
    typedef Range1_< F64 >  double_range;

};


#endif //WONDERSTEWENGINE_DATA_POINT2D_H