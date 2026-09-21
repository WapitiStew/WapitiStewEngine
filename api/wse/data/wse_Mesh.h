//*****************************************************************************************************************
//! 
//! @file    wse_Mesh.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Feb-17, 2025 Create New  WapitiStew.
//!
//!
//! @brief
//!     \~japanese Mesh.h はメッシュデータ構造と関連型を定義するファイル。
//!     \~english  Mesh.h defines mesh data structures and related types.
//!
//!
//! @details
//!     \~japanese
//!         このファイルでは、三角形メッシュやグリッドメッシュを表す構造体 sMeshTriabgle, sMeshGrid を提供する。
//!         また、レンダラー用の汎用メッシュクラス Mesh_ を実装し、頂点アクセスやメッシュサイズ取得などの機能をサポートする。
//!     \~english
//!         This file provides structures sMeshTriabgle and sMeshGrid representing triangle and grid meshes.
//!         It also implements a generic mesh class Mesh_ for renderer usage, supporting vertex access and mesh size retrieval.
//!
//!
//! @note
//!     \~japanese メッシュのインデックス範囲チェックには std::out_of_range を使用する。
//!     \~english  Uses std::out_of_range for mesh index range checks.
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
#ifndef WONDERSTEWENGINE_DATA_MESH_H
#define WONDERSTEWENGINE_DATA_MESH_H


// include files
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#endif
#include <stdexcept>
#include "../depend/wse_STD.h"
#include "../depend/wse_Typedef.h"

#include "wse_TiePoint.h"
#include "wse_Point2D.h"
#include "wse_Point3D.h"
#include "wse_Size.h"
#include "wse_Map.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace wse
{

    //!
    //! @struct sMeshTriabgle
    //!
    //! @brief
    //!     \~japanese 3つの頂点を持つ三角形メッシュを表すテンプレート構造体。
    //!     \~english  Template struct representing a triangle mesh with three vertices.
    //!
    //! @details
    //!     \~japanese
    //!         _sTp 型のソース座標と _dTp 型の変換後座標を持つ TiePoint_ を 3 つ格納する。
    //!         各頂点には対応するインデックスを保持する。
    //!     \~english
    //!         Stores three TiePoint_ objects with source type _sTp and destination type _dTp.
    //!         Each vertex maintains its corresponding index.
    //!
    template< typename _sTp, typename _dTp >
    struct sMeshTriabgle
    {
        public: static constexpr size_t VERTEX_NUM = 3;                     //!< \~english Number of vertices per triangle. \~japanese 三角形あたりの頂点数.
        public: std::array< TiePoint_< _sTp, _dTp >, VERTEX_NUM > vertex;   //!< \~english Array of TiePoint_ vertices.     \~japanese TiePoint_ 頂点配列.
        public: std::array< size_t, VERTEX_NUM >                  index;    //!< \~english Array of vertex indices.         \~japanese 頂点インデックス配列.
    };
  
    static constexpr size_t MESH_LEFT_TOP       = 0; //!< \~english Top-left vertex index.       \~japanese 左上頂点のインデックス.
    static constexpr size_t MESH_RIGHT_TOP      = 1; //!< \~english Top-right vertex index.      \~japanese 右上頂点のインデックス.
    static constexpr size_t MESH_RIGHT_BOTTOM   = 2; //!< \~english Bottom-right vertex index.   \~japanese 右下頂点のインデックス.
    static constexpr size_t MESH_LEFT_BOTTOM    = 3; //!< \~english Bottom-left vertex index.    \~japanese 左下頂点のインデックス.
    static constexpr size_t MESH_VERTEX_NUM     = 4; //!< \~english Number of vertices per grid. \~japanese グリッドあたりの頂点数.

    //!
    //! @struct sMeshGrid
    //!
    //! @brief
    //!     \~japanese 4頂点を持つグリッドメッシュを表すテンプレート構造体。
    //!     \~english  Template struct representing a grid mesh with four vertices.
    //!
    //! @details
    //!     \~japanese
    //!         左上、右上、右下、左下の4つの頂点を TiePoint_ として保持し、
    //!         それぞれにインデックスを割り当てる。
    //!         三角形メッシュを取得するメソッドを提供する。
    //!     \~english
    //!         Maintains four vertices (top-left, top-right, bottom-right, bottom-left) as TiePoint_ objects,
    //!         each with its index. Provides methods to retrieve triangle meshes.
    //!
    template< typename _sTp, typename _dTp >
    struct sMeshGrid
    {
        public : using VertexArray = std::array< TiePoint_< _sTp, _dTp >, MESH_VERTEX_NUM >;
        public : using IndexArray  = std::array< size_t, MESH_VERTEX_NUM >                 ;

        public : VertexArray vertex;    //!< \~english Array of TiePoint_ vertices. \~japanese TiePoint_ 頂点配列。
        public : IndexArray  index ;    //!< \~english Array of vertex indices. \~japanese 頂点インデックス配列。

        //!
        //! @brief
        //!     \~japanese 右上三角形を取得する。
        //!     \~english  Get the top-right triangle mesh.
        //!
        //! @return Triangle mesh (sMeshTriabgle&lt;_sTp,_dTp&gt;).
        //!
        public: sMeshTriabgle< _sTp, _dTp > getMeshTriangle_RT()const
        {
            sMeshTriabgle< _sTp, _dTp > tri;
            tri.vertex[0] = vertex[ MESH_LEFT_TOP     ];
            tri.vertex[1] = vertex[ MESH_RIGHT_TOP    ];
            tri.vertex[2] = vertex[ MESH_RIGHT_BOTTOM ];

            tri.index [0] = index[ MESH_LEFT_TOP     ];
            tri.index [1] = index[ MESH_RIGHT_TOP    ];
            tri.index [2] = index[ MESH_RIGHT_BOTTOM ];

            return tri;
        }

        //!
        //! @brief
        //!     \~japanese 右上三角形を取得する。
        //!     \~english  Get the top-right triangle mesh.
        //!
        //! @return Triangle mesh (sMeshTriabgle&lt;_sTp,_dTp&gt;).
        //!
        public: sMeshTriabgle< _sTp, _dTp > getTriangleVertex_LB()const
        {
            sMeshTriabgle< _sTp, _dTp > tri;
            tri.vertex[0] = vertex[ MESH_RIGHT_BOTTOM ];
            tri.vertex[1] = vertex[ MESH_LEFT_BOTTOM  ];
            tri.vertex[2] = vertex[ MESH_LEFT_TOP     ];

            tri.index [0] = index[ MESH_RIGHT_BOTTOM ];
            tri.index [1] = index[ MESH_LEFT_BOTTOM  ];
            tri.index [2] = index[ MESH_LEFT_TOP     ];

            return tri;
        }
        
        //!
        //! @brief
        //!     \~japanese 左上三角形を取得する。
        //!     \~english  Get the top-left triangle mesh.
        //!
        //! @return Triangle mesh (sMeshTriabgle&lt;_sTp,_dTp&gt;).
        //!
        public: sMeshTriabgle< _sTp, _dTp > getTriangleVertex_LT()const
        {
            sMeshTriabgle< _sTp, _dTp > tri;
            tri.vertex[0] = vertex[ MESH_RIGHT_BOTTOM ];
            tri.vertex[1] = vertex[ MESH_LEFT_TOP     ];
            tri.vertex[2] = vertex[ MESH_RIGHT_TOP    ];

            tri.index [0] = index[ MESH_RIGHT_BOTTOM ];
            tri.index [1] = index[ MESH_LEFT_TOP     ];
            tri.index [2] = index[ MESH_RIGHT_TOP    ];

            return tri;
        }
        
        //!
        //! @brief
        //!     \~japanese 右下三角形を取得する。
        //!     \~english  Get the bottom-right triangle mesh.
        //!
        //! @return Triangle mesh (sMeshTriabgle&lt;_sTp,_dTp&gt;).
        //!
        public: sMeshTriabgle< _sTp, _dTp > getTriangleVertex_RB()const
        {

            sMeshTriabgle< _sTp, _dTp > tri;
            tri.vertex[0] = vertex[ MESH_RIGHT_TOP    ];
            tri.vertex[1] = vertex[ MESH_RIGHT_BOTTOM ];
            tri.vertex[2] = vertex[ MESH_LEFT_BOTTOM  ];

            tri.index [0] = index[ MESH_RIGHT_TOP    ];
            tri.index [1] = index[ MESH_RIGHT_BOTTOM ];
            tri.index [2] = index[ MESH_LEFT_BOTTOM  ];

            return tri;
        }
    };

    //!
    //! @class  Mesh_
    //!
    //! @brief
    //!     \~japanese レンダラ用の汎用メッシュデータクラス。
    //!     \~english  Generic mesh data class for renderer usage.
    //!
    //! @details
    //!     \~japanese
    //!         TiePoint_ 型の頂点マップを保持し、行列形式でアクセス可能にする。
    //!         頂点幅・高さ、メッシュ幅・高さの取得や、指定グリッドの三角形メッシュ取得メソッドを提供する。
    //!         ソースおよびデスティネーションの座標リスト取得機能も備える。
    //!     \~english
    //!         Maintains a map of TiePoint_ vertices, accessible in a matrix format.
    //!         Provides methods to get vertex width/height, mesh width/height, and retrieve grid triangle meshes.
    //!         Also supports retrieving source and destination coordinate lists.
    //!
    //! @note
    //!     \~japanese コピーやムーブ操作では swap 関数を使用して安全にデータを入れ替える。
    //!     \~english  Use swap function for safe data exchange during copy or move operations.
    //!
    template< typename _sTp, typename _dTp >
    struct Mesh_
    {
        //-----------------------------------------------------------------------------------------
        // メンバ.
        //-----------------------------------------------------------------------------------------
        private : Map < TiePoint_< _sTp, _dTp > > m_vertexes;       //!< \~english Vertex map storing TiePoint_ objects. \~japanese TiePoint_ オブジェクトを格納する頂点マップ。

        //! 
        //! @brief
        //!     \~japanese Mesh_ オブジェクト同士を交換する swap 関数。
        //!     \~english  Swap function for Mesh_ objects.
        //!
        //! @param [in,out] obj1_inout  First Mesh_ pointer.
        //! @param [in,out] obj2_inout  Second Mesh_ pointer.
        //!
        friend void swap( Mesh_* obj1_inout, Mesh_* obj2_inout )
        {
            std::swap( obj1_inout->m_vertexes, obj2_inout->m_vertexes );
        }
        
        //----------------------------------------------------------------------
        // アクセッサ.
        //----------------------------------------------------------------------

        //!
        //! @brief
        //!     \~japanese 指定行の頂点配列へのポインタを取得する。
        //!     \~english  Get pointer to the vertex array for the specified row.
        //!
        //! @param[in] y_in  Row index.
        //! @return Pointer to TiePoint_ array.
        //!
        //! @pre \~english Index is valid; access is unchecked, non-owning, and performs no copy.
        //!      \~japanese 有効範囲の添字を指定する。検査・所有権移動・コピーは行わない。
        public : template< typename T, std::enable_if_t<std::is_integral<T>::value, int > = 0>
        inline TiePoint_< _sTp, _dTp >* operator[]( const T y_in ) noexcept
        {
            return this->m_vertexes[ y_in ];
        }

        //!
        //! @brief
        //!     \~japanese 指定行の頂点配列へのポインタを取得する。
        //!     \~english  Get pointer to the vertex array for the specified row.
        //!
        //! @param[in] y_in  Row index.
        //! @return Pointer to TiePoint_ array.
        //!
        //! @pre \~english Index is valid; access is unchecked, non-owning, and performs no copy.
        //!      \~japanese 有効範囲の添字を指定する。検査・所有権移動・コピーは行わない。
        public : template< typename T, std::enable_if_t<std::is_integral<T>::value, int > = 0>
        inline const TiePoint_< _sTp, _dTp >* operator[]( const T y_in ) const noexcept
        {
            return this->m_vertexes[ y_in ];
        }
        

        //-----------------------------------------------------------------------------------------
        // ゲッター.
        //-----------------------------------------------------------------------------------------

        //!
        //! @brief
        //!     \~japanese 頂点の幅（列数）を取得する。
        //!     \~english  Get the vertex width (number of columns).
        //!
        //! @return Width as size_t.
        //!
        public : size_t vertex_width ( void ) const { return this->m_vertexes.width() ; }

        //!
        //! @brief
        //!     \~japanese 頂点の高さ（行数）を取得する。
        //!     \~english  Get the vertex height (number of rows).
        //!
        //! @return Height as size_t.
        //!
        public : size_t vertex_height( void ) const { return this->m_vertexes.height(); }

        //!
        //! @brief
        //!     \~japanese メッシュの幅（vertex_width - 1）を取得する。
        //!     \~english  Get the mesh width (vertex_width - 1).
        //!
        //! @return Mesh width as size_t.
        //!
        public : size_t mesh_width   ( void ) const
        {
            return vertex_width() == 0 ? 0 : vertex_width() - 1;
        }

        //!
        //! @brief
        //!     \~japanese メッシュの高さ（vertex_height - 1）を取得する。
        //!     \~english  Get the mesh height (vertex_height - 1).
        //!
        //! @return Mesh height as size_t.
        //!
        public : size_t mesh_height  ( void ) const
        {
            return vertex_height() == 0 ? 0 : vertex_height() - 1;
        }

        //!
        //! @brief
        //!     \~japanese 内部使用: 指定位置のデータインデックスを計算する。
        //!     \~english  Internal: Calculate data index for specified row and column.
        //!
        //! @param[in] row_in  Row index.
        //! @param[in] col_in  Column index.
        //! @return Calculated data index.
        //!
        private : size_t getDataIndex( const size_t row_in, const size_t col_in )
        {
            return ( row_in * ( this->vertex_width() ) ) + col_in;
        }

        //!
        //! @brief
        //!     \~japanese 指定グリッドに対応する 4 頂点からグリッドメッシュを取得する。
        //!     \~english  Get grid mesh from four vertices corresponding to specified grid.
        //!
        //! @param[in] row_in  Grid row index.
        //! @param[in] col_in  Grid column index.
        //! @return sMeshGrid<_sTp,_dTp> structure.
        //!
        //! @note
        //!     \~english  Throws std::out_of_range if row_in or col_in is out of mesh range.
        //!
        public: sMeshGrid< _sTp, _dTp > getGrid( const size_t row_in, const size_t col_in )
        {
            if( row_in >= this->mesh_height() || col_in >= this->mesh_width() ){ throw std::out_of_range( "the grid position is outside the mesh" ); }

            const size_t left   = col_in + 0;
            const size_t right  = col_in + 1;
            const size_t top    = row_in + 0;
            const size_t bottom = row_in + 1;

            sMeshGrid< _sTp, _dTp > grid;
            grid.vertex[ MESH_LEFT_TOP     ] = this->m_vertexes[ top    ][ left  ];
            grid.vertex[ MESH_RIGHT_TOP    ] = this->m_vertexes[ top    ][ right ];
            grid.vertex[ MESH_RIGHT_BOTTOM ] = this->m_vertexes[ bottom ][ right ];
            grid.vertex[ MESH_LEFT_BOTTOM  ] = this->m_vertexes[ bottom ][ left  ];

            grid.index[ MESH_LEFT_TOP     ]  = getDataIndex( top   , left  );
            grid.index[ MESH_RIGHT_TOP    ]  = getDataIndex( top   , right );
            grid.index[ MESH_RIGHT_BOTTOM ]  = getDataIndex( bottom, right );
            grid.index[ MESH_LEFT_BOTTOM  ]  = getDataIndex( bottom, left  );

            return grid;
        }

        //!
        //! @brief
        //!     \~japanese メッシュサイズ（幅 x 高さ）を uint64_size で取得する。
        //!     \~english  Get mesh size (width x height) as uint64_size.
        //!
        //! @return Mesh dimensions as uint64_size.
        //!
        public: uint64_size mesh_size( void ) const
        {
            return uint64_size( this->mesh_width(), this->mesh_height() );
        }

        //!
        //! @brief
        //!     \~japanese 頂点マップサイズ（幅 x 高さ）を uint64_size で取得する。
        //!     \~english  Get vertex map size (width x height) as uint64_size.
        //!
        //! @return Vertex map dimensions as uint64_size.
        //!
        public: uint64_size vertex_size( void )const
        {
            return uint64_size( this->vertex_width(), this->vertex_height() );
        }

        //!
        //! @brief
        //!     \~japanese ソース座標のリストを取得する。
        //!     \~english  Get list of source coordinates.
        //!
        //! @return std::vector<_sTp> containing source values.
        //!
        public: std::vector< _sTp > getSrcList( void )const 
        {
            std::vector< _sTp > result;
            for( size_t y = 0; y < this->m_vertexes.height(); ++y )
            {
                for( size_t x = 0; x < this->m_vertexes.width(); ++x )
                {
                    result.emplace_back( this->m_vertexes[ y ][ x ].src );
                }
            }
            return result;
        }

        //!
        //! @brief
        //!     \~japanese デスティネーション座標のリストを取得する。
        //!     \~english  Get list of destination coordinates.
        //!
        //! @return std::vector<_dTp> containing destination values.
        //!
        public: std::vector< _dTp > getDstList( void )const
        {
            std::vector< _dTp > result;
            for( size_t y = 0; y < this->m_vertexes.height(); ++y )
            {
                for( size_t x = 0; x < this->m_vertexes.width(); ++x )
                {
                    result.emplace_back( this->m_vertexes[ y ][ x ].dst );
                }
            }
            return result;
        }

        //-----------------------------------------------------------------------------------------
        // コンストラクタ&デストラクタ.
        //-----------------------------------------------------------------------------------------
        
        //! 
        //! @brief Default constructor.
        //! 
        public : explicit
        Mesh_( void )
            : m_vertexes ()
        {
        }
        //!
        //! @brief Constructor with rows and columns.
        //! @param[in] rows_in Number of rows.
        //! @param[in] cols_in Number of columns.
        //!
        public : explicit
        Mesh_( const size_t rows_in, const size_t cols_in )
            : m_vertexes ( cols_in, rows_in )
        {
        }
        //! 
        //! @brief Destructor.
        //! 
        public :
        ~Mesh_( void )
        { }
           
        //! 
        //! @brief Copy constructor.
        //! @param[in] obj_in Source object.
        //! 
        public : Mesh_( const Mesh_< _sTp, _dTp >&obj_in  )
            : m_vertexes ( obj_in.m_vertexes )
        {
        }
        //! 
        //! @brief Move constructor.
        //! @param [in,out] obj_inout Source object.
        //! 
        public : Mesh_( Mesh_< _sTp, _dTp >&&obj_inout )noexcept
            : m_vertexes ( std::move( obj_inout.m_vertexes ) )
        {
        }
        //! 
        //! @brief Copy assignment operator.
        //! @param[in] obj Source object.
        //! @return Reference to this object.
        //! 
        public : Mesh_& operator = ( const Mesh_< _sTp, _dTp > &obj  )
        {
            Mesh_ src( obj );
            swap( this, &src );
            return *this;
        }
    
        //!
        //! @brief Move assignment operator.
        //! @param[in] obj Source object.
        //! @return Reference to this object.
        //! 
        public : Mesh_& operator = ( Mesh_<_sTp, _dTp>&&obj )noexcept
        {
            Mesh_ src( std::move( obj ) );
            swap( this, &src );
            return *this;
        }
    };

    typedef Mesh_<  sint08_xy,  sint08_xy >    sint08_mesh2d;   //!< \~english 2D mesh with sint08_xy coordinates. \~japanese sint08_xy 座標の2Dメッシュ。
    typedef Mesh_<  sint16_xy,  sint16_xy >    sint16_mesh2d;   //!< \~english 2D mesh with sint16_xy coordinates. \~japanese sint16_xy 座標の2Dメッシュ。
    typedef Mesh_<  sint32_xy,  sint32_xy >    sint32_mesh2d;   //!< \~english 2D mesh with sint32_xy coordinates. \~japanese sint32_xy 座標の2Dメッシュ。
    typedef Mesh_<  sint64_xy,  sint64_xy >    sint64_mesh2d;   //!< \~english 2D mesh with sint64_xy coordinates. \~japanese sint64_xy 座標の2Dメッシュ。
    typedef Mesh_<  uint08_xy,  uint08_xy >    uint08_mesh2d;   //!< \~english 2D mesh with uint08_xy coordinates. \~japanese uint08_xy 座標の2Dメッシュ。
    typedef Mesh_<  uint16_xy,  uint16_xy >    uint16_mesh2d;   //!< \~english 2D mesh with uint16_xy coordinates. \~japanese uint16_xy 座標の2Dメッシュ。
    typedef Mesh_<  uint32_xy,  uint32_xy >    uint32_mesh2d;   //!< \~english 2D mesh with uint32_xy coordinates. \~japanese uint32_xy 座標の2Dメッシュ。
    typedef Mesh_<  uint64_xy,  uint64_xy >    uint64_mesh2d;   //!< \~english 2D mesh with uint64_xy coordinates. \~japanese uint64_xy 座標の2Dメッシュ。
    typedef Mesh_< float32_xy, float32_xy >   float32_mesh2d;  //!< \~english 2D mesh with float32_xy coordinates. \~japanese float32_xy 座標の2Dメッシュ。
    typedef Mesh_< float64_xy, float64_xy >   float64_mesh2d;  //!< \~english 2D mesh with float64_xy coordinates. \~japanese float64_xy 座標の2Dメッシュ。
    typedef Mesh_< float32_xy, float32_xy >     float_mesh2d;   //!< \~english 2D mesh with float32_xy coordinates. \~japanese float32_xy 座標の2Dメッシュ。
    typedef Mesh_<  double_xy,  double_xy >    double_mesh2d;   //!< \~english 2D mesh with double_xy coordinates. \~japanese double_xy 座標の2Dメッシュ。

    typedef Mesh_<  sint08_xyz,  sint08_xyz >  sint08_mesh3d;   //!< \~english 3D mesh with sint08_xyz coordinates. \~japanese sint08_xyz 座標の3Dメッシュ。
    typedef Mesh_<  sint16_xyz,  sint16_xyz >  sint16_mesh3d;   //!< \~english 3D mesh with sint16_xyz coordinates. \~japanese sint16_xyz 座標の3Dメッシュ。
    typedef Mesh_<  sint32_xyz,  sint32_xyz >  sint32_mesh3d;   //!< \~english 3D mesh with sint32_xyz coordinates. \~japanese sint32_xyz 座標の3Dメッシュ。
    typedef Mesh_<  sint64_xyz,  sint64_xyz >  sint64_mesh3d;   //!< \~english 3D mesh with sint64_xyz coordinates. \~japanese sint64_xyz 座標の3Dメッシュ。
    typedef Mesh_<  uint08_xyz,  uint08_xyz >  uint08_mesh3d;   //!< \~english 3D mesh with uint08_xyz coordinates. \~japanese uint08_xyz 座標の3Dメッシュ。
    typedef Mesh_<  uint16_xyz,  uint16_xyz >  uint16_mesh3d;   //!< \~english 3D mesh with uint16_xyz coordinates. \~japanese uint16_xyz 座標の3Dメッシュ。
    typedef Mesh_<  uint32_xyz,  uint32_xyz >  uint32_mesh3d;   //!< \~english 3D mesh with uint32_xyz coordinates. \~japanese uint32_xyz 座標の3Dメッシュ。
    typedef Mesh_<  uint64_xyz,  uint64_xyz >  uint64_mesh3d;   //!< \~english 3D mesh with uint64_xyz coordinates. \~japanese uint64_xyz 座標の3Dメッシュ。
    typedef Mesh_< float32_xyz, float32_xyz > float32_mesh3d;  //!< \~english 3D mesh with float32_xyz coordinates. \~japanese float32_xyz 座標の3Dメッシュ。
    typedef Mesh_< float64_xyz, float64_xyz > float64_mesh3d;  //!< \~english 3D mesh with float64_xyz coordinates. \~japanese float64_xyz 座標の3Dメッシュ。
    typedef Mesh_< float32_xyz, float32_xyz >   float_mesh3d;  //!< \~english 3D mesh with float32_xyz coordinates. \~japanese float32_xyz 座標の3Dメッシュ。
    typedef Mesh_<  double_xyz,  double_xyz >  double_mesh3d;   //!< \~english 3D mesh with double_xyz coordinates. \~japanese double_xyz 座標の3Dメッシュ。

    typedef Mesh_<  sint08_xy,  sint08_xyz >  sint08_mesh_2d3d; //!< \~english 2D-to-3D mesh with sint08 types. \~japanese sint08_xy から sint08_xyz へのメッシュ。
    typedef Mesh_<  sint16_xy,  sint16_xyz >  sint16_mesh_2d3d; //!< \~english 2D-to-3D mesh with sint16 types. \~japanese sint16_xy から sint16_xyz へのメッシュ。
    typedef Mesh_<  sint32_xy,  sint32_xyz >  sint32_mesh_2d3d; //!< \~english 2D-to-3D mesh with sint32 types. \~japanese sint32_xy から sint32_xyz へのメッシュ。
    typedef Mesh_<  sint64_xy,  sint64_xyz >  sint64_mesh_2d3d; //!< \~english 2D-to-3D mesh with sint64 types. \~japanese sint64_xy から sint64_xyz へのメッシュ。
    typedef Mesh_<  uint08_xy,  uint08_xyz >  uint08_mesh_2d3d; //!< \~english 2D-to-3D mesh with uint08 types. \~japanese uint08_xy から uint08_xyz へのメッシュ。
    typedef Mesh_<  uint16_xy,  uint16_xyz >  uint16_mesh_2d3d; //!< \~english 2D-to-3D mesh with uint16 types. \~japanese uint16_xy から uint16_xyz へのメッシュ。
    typedef Mesh_<  uint32_xy,  uint32_xyz >  uint32_mesh_2d3d; //!< \~english 2D-to-3D mesh with uint32 types. \~japanese uint32_xy から uint32_xyz へのメッシュ。
    typedef Mesh_<  uint64_xy,  uint64_xyz >  uint64_mesh_2d3d; //!< \~english 2D-to-3D mesh with uint64 types. \~japanese uint64_xy から uint64_xyz へのメッシュ。
    typedef Mesh_< float32_xy, float32_xyz > float32_mesh_2d3d; //!< \~english 2D-to-3D mesh with float32 types. \~japanese float32_xy から float32_xyz へのメッシュ。
    typedef Mesh_< float64_xy, float64_xyz > float64_mesh_2d3d; //!< \~english 2D-to-3D mesh with float64 types. \~japanese float64_xy から float64_xyz へのメッシュ。
    typedef Mesh_< float32_xy, float32_xyz >   float_mesh_2d3d; //!< \~english 2D-to-3D mesh with float32 types. \~japanese float32_xy から float32_xyz へのメッシュ。
    typedef Mesh_<  double_xy,  double_xyz >  double_mesh_2d3d; //!< \~english 2D-to-3D mesh with double types. \~japanese double_xy から double_xyz へのメッシュ。

    typedef Mesh_<  sint08_xyz,  sint08_xy >  sint08_mesh_3d2d; //!< \~english 3D-to-2D mesh with sint08 types. \~japanese sint08_xyz から sint08_xy へのメッシュ。
    typedef Mesh_<  sint16_xyz,  sint16_xy >  sint16_mesh_3d2d; //!< \~english 3D-to-2D mesh with sint16 types. \~japanese sint16_xyz から sint16_xy へのメッシュ。
    typedef Mesh_<  sint32_xyz,  sint32_xy >  sint32_mesh_3d2d; //!< \~english 3D-to-2D mesh with sint32 types. \~japanese sint32_xyz から sint32_xy へのメッシュ。
    typedef Mesh_<  sint64_xyz,  sint64_xy >  sint64_mesh_3d2d; //!< \~english 3D-to-2D mesh with sint64 types. \~japanese sint64_xyz から sint64_xy へのメッシュ。
    typedef Mesh_<  uint08_xyz,  uint08_xy >  uint08_mesh_3d2d; //!< \~english 3D-to-2D mesh with uint08 types. \~japanese uint08_xyz から uint08_xy へのメッシュ。
    typedef Mesh_<  uint16_xyz,  uint16_xy >  uint16_mesh_3d2d; //!< \~english 3D-to-2D mesh with uint16 types. \~japanese uint16_xyz から uint16_xy へのメッシュ。
    typedef Mesh_<  uint32_xyz,  uint32_xy >  uint32_mesh_3d2d; //!< \~english 3D-to-2D mesh with uint32 types. \~japanese uint32_xyz から uint32_xy へのメッシュ。
    typedef Mesh_<  uint64_xyz,  uint64_xy >  uint64_mesh_3d2d; //!< \~english 3D-to-2D mesh with uint64 types. \~japanese uint64_xyz から uint64_xy へのメッシュ。
    typedef Mesh_< float32_xyz, float32_xy > float32_mesh_3d2d; //!< \~english 3D-to-2D mesh with float32 types. \~japanese float32_xyz から float32_xy へのメッシュ。
    typedef Mesh_< float64_xyz, float64_xy > float64_mesh_3d2d; //!< \~english 3D-to-2D mesh with float64 types. \~japanese float64_xyz から float64_xy へのメッシュ。
    typedef Mesh_< float32_xyz, float32_xy >   float_mesh_3d2d; //!< \~english 3D-to-2D mesh with float32 types. \~japanese float32_xyz から float32_xy へのメッシュ。
    typedef Mesh_<  double_xyz,  double_xy >  double_mesh_3d2d; //!< \~english 3D-to-2D mesh with double types. \~japanese double_xyz から double_xy へのメッシュ。


}


#endif  //WONDERSTEWENGINE_DATA_MESH_H
