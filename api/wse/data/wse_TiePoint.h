//*****************************************************************************************************************
//! 
//! @file    wse_TiePoint.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese タイポイント（対応点）構造体のテンプレート定義。
//!     \~english  Template definition for tiepoints (correspondence points).
//!
//!
//! @details
//!     \~japanese このファイルでは、2D・3D座標間の対応を表す汎用テンプレート構造体 TiePoint_ を定義しています。
//!               テンプレートパラメータとして任意の座標型を与えることで、2次元同士、2D-3D、3D同士の対応情報を保持できます。
//!     \~english  This file defines a generic template struct TiePoint_ representing correspondences between 2D and 3D points.
//!               It supports any coordinate types for source and destination, such as 2D-2D, 2D-3D, or 3D-3D mappings.
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

#ifndef WONDERSTEWENGINE_DATA_TIEPOINTS_H
#define WONDERSTEWENGINE_DATA_TIEPOINTS_H


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // 相対パスに../をつけること.
#endif
// include files
#include "../depend/wse_STD.h"
#include "../depend/wse_Typedef.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "wse_Point2D.h"
#include "wse_Point3D.h"

// External Library




namespace wse
{

    //! 
    //! @class TiePoint_
    //! @brief
    //!     \~japanese 任意の型の対応点を保持するテンプレート構造体。
    //!     \~english  Template struct for holding corresponding points of arbitrary types.
    //!
    //! @details
    //!     \~japanese src（入力座標）と dst（出力座標）を1組として保持します。2D-2D, 2D-3D, 3D-2D, 3D-3D などの用途に対応します。
    //!     \~english  Stores a pair of coordinates as src (source) and dst (destination). Supports 2D-2D, 2D-3D, 3D-2D, or 3D-3D cases.
    //!
    template< typename _sTp, typename _dTp >
    struct TiePoint_
    {
        //---------------------------------------
        // Member valiables
        //---------------------------------------
        public: _sTp src;  //!< \~english Source coordinate.      \~japanese 入力側座標.
        public: _dTp dst;  //!< \~english Destination coordinate. \~japanese 出力側座標.


        //! 
        //! @brief
        //!     \~japanese スワップ処理。
        //!     \~english  Swap function.
        //! @param [in,out] obj1_inout \~english First object.  \~japanese オブジェクト1
        //! @param [in,out] obj2_inout \~english Second object. \~japanese オブジェクト2
        //!
        friend void swap ( TiePoint_< _sTp, _dTp >& obj1_inout, TiePoint_< _sTp, _dTp >& obj2_inout )
        {
            std::swap(obj1_inout.src, obj2_inout.src );
            std::swap(obj1_inout.dst, obj2_inout.dst);
        }      

        //---------------------------------------
        // Constractor / Destractor.
        //---------------------------------------

        //! 
        //! @brief Default constructor
        //! 
        public: explicit TiePoint_( void )
            : src ()
            , dst (){}

        //! 
        //! @brief Destructor
        //! 
        public: ~TiePoint_( void ){}

        //! 
        //! @brief Copy constructor
        //! @param [in] obj_in Source object
        //! 
        public: TiePoint_( const TiePoint_< _sTp, _dTp >& obj_in )
            : src ( obj_in.src )
            , dst ( obj_in.dst ){}

        //! 
        //! @brief Move constructor
        //! @param [in] obj_inout Source object
        //! 
        public:
        TiePoint_( TiePoint_< _sTp, _dTp >&& obj_inout )noexcept
            : src ( std::move( obj_inout.src ) )
            , dst ( std::move( obj_inout.dst ) ){}

        //! 
        //! @brief Copy assignment operator
        //! @param [in] obj Source object
        //! 
        public:
        TiePoint_< _sTp, _dTp >& operator = ( const TiePoint_< _sTp, _dTp >& obj )
        {
            TiePoint_< _sTp, _dTp > copy_obj(obj);
            swap(*this, copy_obj);
            return *this;
        }

        //! 
        //! @brief Move assignment operator
        //! @param [in] obj Source object
        //! 
        public:
        TiePoint_< _sTp, _dTp >& operator = ( TiePoint_< _sTp, _dTp >&& obj )noexcept
        {
            TiePoint_< _sTp, _dTp > move_obj(std::move(obj));
            swap(*this, move_obj);
            return *this;
        }


        //---------------------------------------
        // Operator.
        //---------------------------------------

        //! 
        //! @brief Equality operator
        //! @param [in] obj Object to compare
        //! 
        public: bool operator == ( const TiePoint_< _sTp, _dTp >& obj )
        {
            return ( this->src == obj.src ) && ( this->dst == obj.dst );
        }

        //! 
        //! @brief Inequality operator
        //! @param [in] obj Object to compare
        //! 
        public: bool operator != ( const TiePoint_< _sTp, _dTp >& obj )
        {
            return !( ( *this ) == obj );
        }
    };

    typedef TiePoint_< float32_xy, float32_xy >   f32_xy_xy;
    typedef TiePoint_< float64_xy, float64_xy >   f64_xy_xy;

    typedef TiePoint_< float32_xy, float32_xyz >  f32_xy_xyz;
    typedef TiePoint_< float64_xy, float64_xyz >  f64_xy_xyz;

    typedef TiePoint_< float32_xyz, float32_xy >  f32_xyz_xy;
    typedef TiePoint_< float64_xyz, float64_xy >  f64_xyz_xy;

    typedef TiePoint_< float32_xyz, float32_xyz > f32_xyz_xyz;
    typedef TiePoint_< float64_xyz, float64_xyz > f64_xyz_xyz;
};


#endif //WONDERSTEWENGINE_DATA_COLOR_H