//*****************************************************************************************************************
//! 
//! @file    wse_Cell.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   2022/06/12   Create New WapitiStew      WapitiStew
//!
//!
//! @brief
//!     \~japanese セルデータ構造体(Cell2D_, Cell3D_)の定義を行うヘッダファイル.
//!     \~english  Header defining cell data structures (Cell2D_, Cell3D_).
//!
//!
//! @details
//!     \~japanese
//!         2Dおよび3D空間における四隅の座標(src/dst)を保持するセル構造をテンプレート形式で定義。
//!         getGridによりsMeshGridへの変換も提供される。
//!
//!     \~english
//!         Defines templated cell structures for 2D and 3D space, storing corner coordinates (src/dst).
//!         Also provides conversion to sMeshGrid format.
//!
//!
//! @note
//!     \~japanese Cell2D_は2次元、Cell3D_は3次元への射影に用いる。
//!     \~english  Cell2D_ is used for 2D and Cell3D_ for 3D projections.
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
#ifndef WONDERSTEWENGINE_DATA_CELL_H
#define WONDERSTEWENGINE_DATA_CELL_H

#include "../depend/wse_STD.h"
#include "../depend/wse_Typedef.h"
#include "../../dynamic.h"

#include "wse_Point2D.h"
#include "wse_Point3D.h"
#include "wse_Mesh.h"

namespace wse
{
    //!
    //! @struct Cell2D_
    //!
    //! @brief
    //!     \~japanese 2次元のセル（格子）を表す構造体テンプレート.
    //!     \~english  Templated structure representing a 2D cell (grid).
    //!
    //! @details
    //!     \~japanese
    //!         2D平面上の4頂点(src, dst)を格納し、メッシュ変換可能な構造体。
    //!         頂点はLEFT_TOPなどのインデックス定数で管理される。
    //!         型はテンプレート引数により任意の数値型に対応。
    //!
    //!     \~english
    //!         Stores 4 corner vertices (src, dst) of a 2D grid cell.
    //!         Vertices are indexed using constants like LEFT_TOP.
    //!         Supports any numeric type via templating.
    //!
    //! @note
    //!     \~japanese sMeshGrid型への変換関数 getGrid() を提供している.
    //!     \~english  Provides conversion to sMeshGrid using getGrid().
    //!
    template< typename TYPE >
    struct WSE_API Cell2D_ final
    {
        // 定数
        public : static constexpr uint64_t LEFT_TOP     = 0; //!< \~english Index for top-left corner.       \~japanese 左上頂点のインデックス.
        public : static constexpr uint64_t RIGHT_TOP    = 1; //!< \~english Index for top-right corner.      \~japanese 右上頂点のインデックス.
        public : static constexpr uint64_t RIGHT_BOTTOM = 2; //!< \~english Index for bottom-right corner.   \~japanese 右下頂点のインデックス.
        public : static constexpr uint64_t LEFT_BOTTOM  = 3; //!< \~english Index for bottom-left corner.    \~japanese 左下頂点のインデックス.
        public : static constexpr uint64_t VERTEX_NUM   = 4; //!< \~english Number of vertices (always 4).   \~japanese 頂点数（常に4）.

               
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251)  // DLL インターフェイスが必要
#endif
        // メンバー変数.
        public : std::array< Point2_< TYPE >, VERTEX_NUM > src; //!< \~english Source vertices. \~japanese 入力頂点.
        public : std::array< Point2_< TYPE >, VERTEX_NUM > dst; //!< \~english Destination vertices. \~japanese 出力頂点.
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

        //!
        //! @brief
        //!     \~japanese メッシュ変換構造体を生成する.
        //!     \~english  Generate a mesh grid structure from the cell.
        //!
        //! @return Mesh grid structure.
        //!
        public: sMeshGrid<  Point2_< TYPE >,  Point2_< TYPE > > getGrid()const
        {
            sMeshGrid<  Point2_< TYPE >,  Point2_< TYPE > > grid;
            grid.vertex[ MESH_LEFT_TOP     ].src = this->src[ LEFT_TOP     ];
            grid.vertex[ MESH_RIGHT_TOP    ].src = this->src[ RIGHT_TOP    ];
            grid.vertex[ MESH_RIGHT_BOTTOM ].src = this->src[ RIGHT_BOTTOM ];
            grid.vertex[ MESH_LEFT_BOTTOM  ].src = this->src[ LEFT_BOTTOM  ];
            grid.vertex[ MESH_LEFT_TOP     ].dst = this->dst[ LEFT_TOP     ];
            grid.vertex[ MESH_RIGHT_TOP    ].dst = this->dst[ RIGHT_TOP    ];
            grid.vertex[ MESH_RIGHT_BOTTOM ].dst = this->dst[ RIGHT_BOTTOM ];
            grid.vertex[ MESH_LEFT_BOTTOM  ].dst = this->dst[ LEFT_BOTTOM  ];

            grid.index[ MESH_LEFT_TOP     ]  = LEFT_TOP    ;
            grid.index[ MESH_RIGHT_TOP    ]  = RIGHT_TOP   ;
            grid.index[ MESH_RIGHT_BOTTOM ]  = RIGHT_BOTTOM;
            grid.index[ MESH_LEFT_BOTTOM  ]  = LEFT_BOTTOM ;

            return grid;
        }

        typedef TYPE value_type;

        //!
        //! @brief Default constructor.
        //!
        public:
        Cell2D_( void )
            : src ()
            , dst ()
        {
            src[ LEFT_TOP     ] = Point2_< TYPE >( static_cast< TYPE >( 0 ), static_cast< TYPE >( 0 ) );
            src[ RIGHT_TOP    ] = Point2_< TYPE >( static_cast< TYPE >( 0 ), static_cast< TYPE >( 0 ) );
            src[ RIGHT_BOTTOM ] = Point2_< TYPE >( static_cast< TYPE >( 0 ), static_cast< TYPE >( 0 ) );
            src[ LEFT_BOTTOM  ] = Point2_< TYPE >( static_cast< TYPE >( 0 ), static_cast< TYPE >( 0 ) );
            dst[ LEFT_TOP     ] = Point2_< TYPE >( static_cast< TYPE >( 0 ), static_cast< TYPE >( 0 ) );
            dst[ RIGHT_TOP    ] = Point2_< TYPE >( static_cast< TYPE >( 0 ), static_cast< TYPE >( 0 ) );
            dst[ RIGHT_BOTTOM ] = Point2_< TYPE >( static_cast< TYPE >( 0 ), static_cast< TYPE >( 0 ) );
            dst[ LEFT_BOTTOM  ] = Point2_< TYPE >( static_cast< TYPE >( 0 ), static_cast< TYPE >( 0 ) );
        }

        //!
        //! @brief Destractor.
        //!
        public : 
        ~Cell2D_( void )
        {
        }
        //! @brief Constructor with source/destination vertices.
        //!
        //! @param[in] src_vertex_in  Array of source vertices.
        //! @param[in] dst_vertex_in  Array of destination vertices.
        //!
        public :
        Cell2D_( 
            const std::array< Point2_< TYPE >, VERTEX_NUM >& src_vertex_in
        ,   const std::array< Point2_< TYPE >, VERTEX_NUM >& dst_vertex_in 
        )
            : src ( src_vertex_in )
            , dst ( dst_vertex_in )
        {
        }
        //!
        //! @brief Constructor with type conversion from different vertex type.
        //!
        //! @param[in] src_vertex_in  Source vertices of different type.
        //! @param[in] dst_vertex_in  Destination vertices of different type.
        //!
        public : template< typename IN_TYPE > explicit
        Cell2D_( 
              const std::array< Point2_< IN_TYPE >, VERTEX_NUM >& src_vertex_in
            , const std::array< Point2_< IN_TYPE >, VERTEX_NUM >& dst_vertex_in 
        )
            : src ()
            , dst ()
        {
            for( size_t i = 0; i < VERTEX_NUM; ++i )
            {
                src[ i ] = static_cast< TYPE >( src_vertex_in[ i ] );
                dst[ i ] = static_cast< TYPE >( dst_vertex_in[ i ] );
            }
        }

        //!
        //! @brief Constructor from different Cell2D_ type.
        //!
        //! @param[in] obj_in  Source object of different type.
        //!
        public : template< typename IN_TYPE > explicit
        Cell2D_( 
            const Cell2D_<IN_TYPE>& obj_in
        )
            : src ( obj_in.src )
            , dst ( obj_in.dst )
        {
            for( size_t i = 0; i < VERTEX_NUM; ++i )
            {
                src[ i ] = static_cast< TYPE >( obj_in.src[ i ] );
                dst[ i ] = static_cast< TYPE >( obj_in.dst[ i ] );
            }
        }

        //!
        //! @brief Copy constructor.
        //!
        //! @param[in] obj_in  Source object.
        //!
        public : 
        Cell2D_( 
            const Cell2D_ &obj_in
        )
            : src ( obj_in.src )
            , dst ( obj_in.dst )
        {
        }
        //!
        //! @brief Move constructor.
        //!
        //! @param[in] obj_inout  Source object to move.
        //!
        public : 
        Cell2D_( 
            Cell2D_ &&obj_inout
        )noexcept 
            : src ( std::move( obj_inout.src  ) )
            , dst ( std::move( obj_inout.dst ) )
        {
        }
        //!
        //! @brief Swap function.
        //!
        //! @param[in,out] obj1_inout  First object.
        //! @param[in,out] obj2_inout  Second object.
        //!
        friend 
        void swap( 
            Cell2D_<TYPE> &obj1_inout
        ,   Cell2D_<TYPE> &obj2_inout
        )
        {
            std::swap(obj1_inout.src, obj2_inout.src);
            std::swap(obj1_inout.dst, obj2_inout.dst);
        }

        //!
        //! @brief Copy assignment operator.
        //!
        //! @param[in] obj  Source object.
        //! @return Reference to this object.
        //!
        public : 
        Cell2D_< TYPE > &operator = ( 
            const Cell2D_<TYPE> &obj
        )
        {
            Cell2D_< TYPE > copy_obj( obj );
            swap( *this, copy_obj );
            return *this;
        }

        //!
        //! @brief Move assignment operator.
        //!
        //! @param[in] obj  Source object to move.
        //! @return Reference to this object.
        //!
        public : 
        Cell2D_< TYPE > &operator = (
            Cell2D_<TYPE> &&obj
        )noexcept
        {
            Cell2D_< TYPE > move_obj( std::move( obj ) );
            swap( *this, move_obj );
            return *this;
        }

        //!
        //! @brief Convert to another Cell2D_ type.
        //!
        //! @param[in] obj  Source object.
        //! @return Converted Cell2D_ object.
        //!
        public : template< typename TYPE1, typename TYPE2 > 
        Cell2D_< TYPE2 > &operator = ( 
            const Cell2D_<TYPE> &obj
        )
        {
            return Cell2D_< TYPE2 >( static_cast< TYPE2 >(obj.src), static_cast< TYPE2 >(obj.dst) );
        }

        //!
        //! @brief
        //!     \~japanese セル構造を文字列に変換する.
        //!     \~english  Convert the cell structure to string.
        //!
        //! @return String representation.
        //!
        public : std::string str( )const
        {
            const std::string os = "LT  SRC=" + this->src[ LEFT_TOP     ].str() + "DST=" + this->dst[ LEFT_TOP     ].str()
                                 + "RT  SRC=" + this->src[ RIGHT_TOP    ].str() + "DST=" + this->dst[ RIGHT_TOP    ].str()
                                 + "RB  SRC=" + this->src[ RIGHT_BOTTOM ].str() + "DST=" + this->dst[ RIGHT_BOTTOM ].str()
                                 + "LM  SRC=" + this->src[ LEFT_BOTTOM  ].str() + "DST=" + this->dst[ LEFT_BOTTOM  ].str();
            return os;
        }

    };

    typedef Cell2D_< int     >   Cell;           //!< \~english Cell with signed int.      \~japanese 整数型のCell.
    typedef Cell2D_< uint8_t  >  uint8_cell;     //!< \~english Cell with uint8_t.         \~japanese 8bit符号なし整数型のCell.
    typedef Cell2D_< uint16_t >  uint16_cell;    //!< \~english Cell with uint16_t.        \~japanese 16bit符号なし整数型のCell.
    typedef Cell2D_< uint32_t >  uint32_cell;    //!< \~english Cell with uint32_t.        \~japanese 32bit符号なし整数型のCell.
    typedef Cell2D_< uint64_t >  uint64_cell;    //!< \~english Cell with uint64_t.        \~japanese 64bit符号なし整数型のCell.
    typedef Cell2D_< int8_t  >   sint8_cell;     //!< \~english Cell with int8_t.          \~japanese 8bit符号あり整数型のCell.
    typedef Cell2D_< int16_t >   sint16_cell;    //!< \~english Cell with int16_t.         \~japanese 16bit符号あり整数型のCell.
    typedef Cell2D_< int32_t >   sint32_cell;    //!< \~english Cell with int32_t.         \~japanese 32bit符号あり整数型のCell.
    typedef Cell2D_< int64_t >   sint64_cell;    //!< \~english Cell with int64_t.         \~japanese 64bit符号あり整数型のCell.
    typedef Cell2D_< float >     float32_cell;   //!< \~english Cell with float (32-bit).  \~japanese 32bit浮動小数点型のCell.
    typedef Cell2D_< double >    float64_cell;   //!< \~english Cell with double (64-bit). \~japanese 64bit浮動小数点型のCell.
    typedef Cell2D_< float >     float_cell;     //!< \~english Alias of float32_cell.     \~japanese 32bit浮動小数点型のCell.
    typedef Cell2D_< double >    double_cell;    //!< \~english Alias of float64_cell.     \~japanese 64bit浮動小数点型のCell.

    //!
    //! @struct Cell3D_
    //!
    //! @brief
    //!     \~japanese 3次元のセル（格子）を表す構造体テンプレート.
    //!     \~english  Templated structure representing a 3D cell (grid).
    //!
    //! @details
    //!     \~japanese
    //!         srcは2次元座標、dstは3次元座標の各4点を持ち、射影変換などに利用可能。
    //!         getGrid()により、sMeshGrid形式への変換も行える。
    //!
    //!     \~english
    //!         Holds 4 2D source points and 4 3D destination points for projection transformations.
    //!         Also convertible to sMeshGrid format using getGrid().
    //!
    //! @note
    //!     \~japanese 変換や可視化に用いるため、基本的にポリゴン単位で処理される.
    //!     \~english  Intended for per-polygon use in projection and visualization tasks.
    //!
    template< typename TYPE >
    struct Cell3D_ final
    {
        // 定数
        public : static constexpr uint64_t LEFT_TOP     = 0; //!< \~english Index for top-left corner.       \~japanese 左上頂点のインデックス.
        public : static constexpr uint64_t RIGHT_TOP    = 1; //!< \~english Index for top-right corner.      \~japanese 右上頂点のインデックス.
        public : static constexpr uint64_t RIGHT_BOTTOM = 2; //!< \~english Index for bottom-right corner.   \~japanese 右下頂点のインデックス.
        public : static constexpr uint64_t LEFT_BOTTOM  = 3; //!< \~english Index for bottom-left corner.    \~japanese 左下頂点のインデックス.
        public : static constexpr uint64_t VERTEX_NUM   = 4; //!< \~english Number of vertices (always 4).   \~japanese 頂点数（常に4）.

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251)  // DLL インターフェイスが必要
#endif
        // メンバー変数.
        public : std::array< Point2_< TYPE >, VERTEX_NUM > src ; //!< \~english Source vertices (2D). \~japanese 入力頂点(2次元).
        public : std::array< Point3_< TYPE >, VERTEX_NUM > dst;  //!< \~english Destination vertices (3D). \~japanese 出力頂点(3次元).
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

        //!
        //! @brief
        //!     \~japanese メッシュ変換構造体を生成する.
        //!     \~english  Generate a mesh grid structure from the 3D cell.
        //!
        //! @return Mesh grid structure.
        //!
        public: sMeshGrid<  Point2_< TYPE >,  Point3_< TYPE > > getGrid()const 
        {
            sMeshGrid<  Point2_< TYPE >,  Point3_< TYPE > > grid;
            grid.vertex[ MESH_LEFT_TOP     ].src = this->src[ LEFT_TOP     ];
            grid.vertex[ MESH_RIGHT_TOP    ].src = this->src[ RIGHT_TOP    ];
            grid.vertex[ MESH_RIGHT_BOTTOM ].src = this->src[ RIGHT_BOTTOM ];
            grid.vertex[ MESH_LEFT_BOTTOM  ].src = this->src[ LEFT_BOTTOM  ];
            grid.vertex[ MESH_LEFT_TOP     ].dst = this->dst[ LEFT_TOP     ];
            grid.vertex[ MESH_RIGHT_TOP    ].dst = this->dst[ RIGHT_TOP    ];
            grid.vertex[ MESH_RIGHT_BOTTOM ].dst = this->dst[ RIGHT_BOTTOM ];
            grid.vertex[ MESH_LEFT_BOTTOM  ].dst = this->dst[ LEFT_BOTTOM  ];

            grid.index[ MESH_LEFT_TOP     ]  = LEFT_TOP    ;
            grid.index[ MESH_RIGHT_TOP    ]  = RIGHT_TOP   ;
            grid.index[ MESH_RIGHT_BOTTOM ]  = RIGHT_BOTTOM;
            grid.index[ MESH_LEFT_BOTTOM  ]  = LEFT_BOTTOM ;

            return grid;
        }

        typedef TYPE value_type;

        //!
        //! @brief Default constructor.
        //!
        public:
        Cell3D_( void )
            : src ()
            , dst ()
        {
        }
        //!
        //! @brief Destructor.
        //!
        public : 
        ~Cell3D_( void )
        {
        }
        //!
        //! @brief Constructor with source/destination vertices.
        //!
        //! @param[in] src_vertex_in  Array of 2D source vertices.
        //! @param[in] dst_vertex_in  Array of 3D destination vertices.
        //!
        public :
        Cell3D_( 
            const std::array< Point2_< TYPE >, VERTEX_NUM >& src_vertex_in
        ,   const std::array< Point3_< TYPE >, VERTEX_NUM >& dst_vertex_in 
        )
            : src ( src_vertex_in )
            , dst ( dst_vertex_in )
        {
        }
        //!
        //! @brief Constructor with type conversion from different vertex type.
        //!
        //! @param[in] src_vertex_in  Source vertices of different type.
        //! @param[in] dst_vertex_in  Destination vertices of different type.
        //!
        public : template< typename IN_TYPE > explicit
        Cell3D_( 
              const std::array< Point2_< IN_TYPE >, VERTEX_NUM >& src_vertex_in
            , const std::array< Point3_< IN_TYPE >, VERTEX_NUM >& dst_vertex_in 
        )
            : src ()
            , dst ()
        {
            for( size_t i = 0; i < VERTEX_NUM; ++i )
            {
                src[ i ] = static_cast< TYPE >( src_vertex_in[ i ] );
                dst[ i ] = static_cast< TYPE >( dst_vertex_in[ i ] );
            }
        }

        //!
        //! @brief Constructor from different Cell3D_ type.
        //!
        //! @param[in] obj_in  Source object of different type.
        //!
        public : template< typename IN_TYPE > explicit
        Cell3D_( 
            const Cell3D_<IN_TYPE>& obj_in
        )
            : src ()
            , dst ()
        {
            for( size_t i = 0; i < VERTEX_NUM; ++i )
            {
                src[ i ] = static_cast< TYPE >( obj_in.src[ i ] );
                dst[ i ] = static_cast< TYPE >( obj_in.dst[ i ] );
            }
        }

        //!
        //! @brief Copy constructor.
        //!
        //! @param[in] obj_in  Source object.
        //!
        public : 
        Cell3D_( 
            const Cell3D_ &obj_in
        )
            : src ( obj_in.src )
            , dst ( obj_in.dst )
        {
        }
        //!
        //! @brief Move constructor.
        //!
        //! @param[in] obj_inout  Source object to move.
        //!
        public : 
        Cell3D_( 
            Cell3D_ &&obj_inout
        )noexcept 
            : src ( std::move( obj_inout.src  ) )
            , dst ( std::move( obj_inout.dst ) )
        {
        }
        //!
        //! @brief Swap function.
        //!
        //! @param[in,out] obj1_inout  First object.
        //! @param[in,out] obj2_inout  Second object.
        //!
        friend 
        void swap( 
            Cell3D_<TYPE> &obj1_inout
        ,   Cell3D_<TYPE> &obj2_inout
        )
        {
            std::swap(obj1_inout.src,       obj2_inout.src);
            std::swap(obj1_inout.dst,       obj2_inout.dst);
        }

        //!
        //! @brief Copy assignment operator.
        //!
        //! @param[in] obj  Source object.
        //! @return Reference to this object.
        //!
        public : 
        Cell3D_< TYPE > &operator = ( 
            const Cell3D_<TYPE> &obj
        )
        {
            Cell3D_< TYPE > copy_obj( obj );
            swap( *this, copy_obj );
            return *this;
        }

        //!
        //! @brief Move assignment operator.
        //!
        //! @param[in] obj  Source object to move.
        //! @return Reference to this object.
        //!
        public : 
        Cell3D_< TYPE > &operator = (
            Cell3D_<TYPE> &&obj
        )
        {
            Cell3D_< TYPE > move_obj( std::move( obj ) );
            swap( *this, move_obj );
            return *this;
        }

        //!
        //! @brief Convert to another Cell3D_ type.
        //!
        //! @param[in] obj  Source object.
        //! @return Converted Cell3D_ object.
        //!
        public : template< typename TYPE1, typename TYPE2 > 
        Cell3D_< TYPE2 > &operator = ( 
            const Cell3D_<TYPE> &obj
        )
        {
            return Cell3D_< TYPE2 >( static_cast< TYPE2 >(obj.src), static_cast< TYPE2 >(obj.dst) );
        }

        //!
        //! @brief
        //!     \~japanese セル構造を文字列に変換する.
        //!     \~english  Convert the cell structure to string.
        //!
        //! @return String representation.
        //!
        public : std::string str( )const
        {
            const std::string os = "LT  SRC=" + this->src[ LEFT_TOP     ].str() + " DST=" + this->dst[ LEFT_TOP     ].str()
                                 + "RT  SRC=" + this->src[ RIGHT_TOP    ].str() + " DST=" + this->dst[ RIGHT_TOP    ].str()
                                 + "RB  SRC=" + this->src[ RIGHT_BOTTOM ].str() + " DST=" + this->dst[ RIGHT_BOTTOM ].str()
                                 + "LM  SRC=" + this->src[ LEFT_BOTTOM  ].str() + " DST=" + this->dst[ LEFT_BOTTOM  ].str();
            return os;
        }

    };

    typedef Cell3D_< int     >   Cell3D;         //!< \~english 3D Cell with signed int.      \~japanese 整数型のCell.
    typedef Cell3D_< uint8_t  >  Cell3DUC;       //!< \~english 3D Cell with uint8_t.         \~japanese 8bit符号なし整数型のCell.
    typedef Cell3D_< uint16_t >  Cell3DUS;       //!< \~english 3D Cell with uint16_t.        \~japanese 16bit符号なし整数型のCell.
    typedef Cell3D_< uint32_t >  Cell3DUI;       //!< \~english 3D Cell with uint32_t.        \~japanese 32bit符号なし整数型のCell.
    typedef Cell3D_< uint64_t >  Cell3DUL;       //!< \~english 3D Cell with uint64_t.        \~japanese 64bit符号なし整数型のCell.
    typedef Cell3D_< int8_t  >   Cell3DC;        //!< \~english 3D Cell with int8_t.          \~japanese 8bit符号あり整数型のCell.
    typedef Cell3D_< int16_t >   Cell3DS;        //!< \~english 3D Cell with int16_t.         \~japanese 16bit符号あり整数型のCell.
    typedef Cell3D_< int32_t >   Cell3DI;        //!< \~english 3D Cell with int32_t.         \~japanese 32bit符号あり整数型のCell.
    typedef Cell3D_< int64_t >   Cell3DL;        //!< \~english 3D Cell with int64_t.         \~japanese 64bit符号あり整数型のCell.
    typedef Cell3D_< float >     Cell3DF;        //!< \~english 3D Cell with float (32-bit).  \~japanese 32bit浮動小数点型のCell.
    typedef Cell3D_< double >    Cell3DD;        //!< \~english 3D Cell with double (64-bit). \~japanese 64bit浮動小数点型のCell.

    typedef Cell3DUC             uint8_cell3d;   //!< \~english Alias of Cell3DUC.            \~japanese 8bit符号なし整数型のCell.
    typedef Cell3DUS            uint16_cell3d;   //!< \~english Alias of Cell3DUS.            \~japanese 16bit符号なし整数型のCell.
    typedef Cell3DUI            uint32_cell3d;   //!< \~english Alias of Cell3DUI.            \~japanese 32bit符号なし整数型のCell.
    typedef Cell3DUL            uint64_cell3d;   //!< \~english Alias of Cell3DUL.            \~japanese 64bit符号なし整数型のCell.
    typedef Cell3DC              sint8_cell3d;   //!< \~english Alias of Cell3DC.             \~japanese 8bit符号あり整数型のCell.
    typedef Cell3DS             sint16_cell3d;   //!< \~english Alias of Cell3DS.             \~japanese 16bit符号あり整数型のCell.
    typedef Cell3DI             sint32_cell3d;   //!< \~english Alias of Cell3DI.             \~japanese 32bit符号あり整数型のCell.
    typedef Cell3DL             sint64_cell3d;   //!< \~english Alias of Cell3DL.             \~japanese 64bit符号あり整数型のCell.
    typedef Cell3DF            float32_cell3d;   //!< \~english Alias of Cell3DF.             \~japanese 32bit浮動小数点型のCell.
    typedef Cell3DD            float64_cell3d;   //!< \~english Alias of Cell3DD.             \~japanese 64bit浮動小数点型のCell.
    typedef Cell3DF              float_cell3d;   //!< \~english Alias of float32_cell3d.      \~japanese 32bit浮動小数点型のCell.
    typedef Cell3DD             double_cell3d;   //!< \~english Alias of float64_cell3d.      \~japanese 64bit浮動小数点型のCell.

    

    //! @brief 文字列表示.
    template< typename _Tp >
    inline static std::ostream& operator << ( std::ostream& os, const Cell2D_< _Tp >& obj )
    {
        os << obj.str();
        return os;
    }
    

    //! @brief 文字列表示.
    template< typename _Tp >
    inline static std::ostream& operator << ( std::ostream& os, const Cell3D_< _Tp >& obj )
    {
        os << obj.str();
        return os;
    }

};

#endif //WONDERSTEWENGINE_DATA_CELL_H
