//*****************************************************************************************************************
//!
//! @file    binController.h
//! @brief   \~japanese 型付き数値配列をWSE独自バイナリ形式で読み書きするBINControllerを定義する。
//! @brief   \~english  Defines BINController for reading and writing typed numeric arrays in the WSE binary format.
//! @author  WapitiStew.
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Aug-30, 2025   Create New     WapitiStew.
//!   Aug-04, 2026   Correct file documentation     WapitiStew.
//!
//! @details
//!   \~japanese 型種別、要素数、数値列から構成されるデータブロックを順序付きで保持し、ファイル入出力する。
//!   \~english  Stores ordered data blocks made of a type, an element count, and numeric values for file I/O.
//! @copyright
//!   Copyright (C) 2025-2026 WapitiStew. All rights reserved.
//!   SPDX-License-Identifier: Apache-2.0
//!   Licensed under the Apache License, Version 2.0; see the LICENSE
//!   file at the repository root.
//!
//!
//*****************************************************************************************************************
#ifndef WONDERSTEWENGINE_UTILITYFORMATFILECONTROLLER_BIN_FILECONTROLLER_H
#define WONDERSTEWENGINE_UTILITYFORMATFILECONTROLLER_BIN_FILECONTROLLER_H


#include <wse/stew.h>


#include <cstdint>
#include <string>
#include <vector>

#include "../error/GefError.h"
#include "../format/SettingFormat.h"
#include "../format/ReadLimits.h"
#include "../../dynamic.h"

#pragma warning(push)
    #pragma warning(disable: 4251)  // Dynamicライブラリでユーザー側に公開されないメンバーが含まれる
namespace wse
{
namespace gef
{
    //!
    //! @brief
    //!     \~japanese 型付き数値配列を順序付きバイナリブロックとして保持する。
    //!     \~english  Holds typed numeric arrays as ordered binary blocks.
    //! @details
    //!     \~japanese データパッケージは型名enum、データ数64bit、各データ値の順で構成する。
    //!     \~english  Each data package consists of a type enum, a 64-bit element count, and element values.
    //!
    class WSE_API BINController
    {

        //----------------------------------
        // Member
        //----------------------------------
        private: std::vector< std::pair< size_t, uint8_t   > >     m_uint08_contents  ;   //!< コンテンツ情報.
        private: std::vector< std::pair< size_t, int8_t    > >     m_int08_contents   ;   //!< コンテンツ情報.
        private: std::vector< std::pair< size_t, int16_t   > >     m_int16_contents   ;   //!< コンテンツ情報.
        private: std::vector< std::pair< size_t, int32_t   > >     m_int32_contents   ;   //!< コンテンツ情報.
        private: std::vector< std::pair< size_t, int64_t   > >     m_int64_contents   ;   //!< コンテンツ情報.
        private: std::vector< std::pair< size_t, float32_t > >     m_float32_contents ;  //!< コンテンツ情報.
        private: std::vector< std::pair< size_t, float64_t > >     m_float64_contents ;  //!< コンテンツ情報.
        private : uint64_t                                         m_header_next_index;
        private : std::vector< std::pair< eCategory, size_t > >    m_order_list;

        //----------------------------------
        // Accessor
        //----------------------------------

        //!
        //! @brief
        //!     \~japanese 最後に格納したBlockのIndexを返す.
        //!     \~english  Returns the index of the newest stored block.
        //!
        //! @exception std::logic_error
        //!     \~japanese Blockを1つも保持していない場合. `index_num()`で事前に確認できる.
        //!     \~english  When no block is stored yet; check `index_num()` first.
        //!
        public : uint64_t last_index ( void ) const;
        public : uint64_t index_num  ( void ) const { return this->m_header_next_index; }

        public: template< typename _Tp > std::vector< _Tp > contents( const size_t index_in )const;

        public: void setContents( const std::vector< uint8_t   >& values_in );
        public: void setContents( const std::vector< int8_t    >& values_in );
        public: void setContents( const std::vector< int16_t   >& values_in );
        public: void setContents( const std::vector< int32_t   >& values_in );
        public: void setContents( const std::vector< int64_t   >& values_in );
        public: void setContents( const std::vector< float32_t >& values_in );
        public: void setContents( const std::vector< float64_t >& values_in );


        //----------------------------------
        // Constractor / Destructor
        //----------------------------------

        //!
        //! @brief  Default constructor.
        //!
        public: BINController();

        //!
        //! @brief  Destructor.
        //!
        public: ~BINController();

        //!
        //! @brief  Copy constructor.
        //!
        //! @param [in ] obj_in Source object.
        //!
        public: BINController( const BINController& obj_in )
            : m_uint08_contents   ( obj_in.m_uint08_contents )
            , m_int08_contents    ( obj_in.m_int08_contents )
            , m_int16_contents    ( obj_in.m_int16_contents )
            , m_int32_contents    ( obj_in.m_int32_contents )
            , m_int64_contents    ( obj_in.m_int64_contents )
            , m_float32_contents  ( obj_in.m_float32_contents )
            , m_float64_contents  ( obj_in.m_float64_contents )
            , m_header_next_index ( obj_in.m_header_next_index )
            , m_order_list        ( obj_in.m_order_list )
        {
        }
        //!
        //! @brief  Move constructor.
        //!
        //! @param [in,out] obj_inout Source object.
        //!
        public:BINController( BINController&& obj_inout )noexcept
            : BINController()
        {
            swap(*this, obj_inout);
        }
        //!
        //! @brief
        //!     \~japanese スワップ処理。
        //!     \~english Swap two objects.
        //!
        //! @param [in,out] obj1_inout Swap object.
        //! @param [in,out] obj2_inout Swap object.
        //!
        friend void swap(BINController& obj1_inout,BINController& obj2_inout ) noexcept
        {
            std::swap(obj1_inout.m_uint08_contents , obj2_inout.m_uint08_contents   );
            std::swap(obj1_inout.m_int08_contents  , obj2_inout.m_int08_contents   );
            std::swap(obj1_inout.m_int16_contents  , obj2_inout.m_int16_contents   );
            std::swap(obj1_inout.m_int32_contents  , obj2_inout.m_int32_contents   );
            std::swap(obj1_inout.m_int64_contents  , obj2_inout.m_int64_contents   );
            std::swap(obj1_inout.m_float32_contents, obj2_inout.m_float32_contents );
            std::swap(obj1_inout.m_float64_contents, obj2_inout.m_float64_contents );
            std::swap(obj1_inout.m_header_next_index, obj2_inout.m_header_next_index);
            std::swap(obj1_inout.m_order_list       , obj2_inout.m_order_list       );
        }


        //----------------------------------
        // Operator
        //----------------------------------

        //!
        //! @brief  Copy assignment operator.
        //!
        //! @param [in ] obj Source object.
        //!
        public:
        BINController& operator = ( const BINController& obj )
        {
            BINController copy_obj(obj);
            swap(*this, copy_obj);
            return *this;
        }

        //!
        //! @brief  Move assignment operator.
        //!
        //! @param [in ] obj Source object.
        //!
        public:
        BINController& operator = ( BINController&& obj )noexcept
        {
            BINController move_obj(std::move(obj));
            swap(*this, move_obj);
            return *this;
        }


        //--------------------------------------------------------------------------------
        // Specific Method
        //--------------------------------------------------------------------------------

        //!
        //! @brief  Read.
        //!
        //! @param [in ] path_in
        //!
        //! @return
        //!     \~japanese 成功`GefStatus`. 開けないFileは`Io/FileOpenFailed`, 途中で切れたBlockは
        //!                `Io/ReadFailed`, 未知のBlock Categoryは`Format/MalformedData`で失敗する.
        //!                成功時に全Blockを置換し、失敗時は以前の状態を保持する。
        //!     \~english  The `GefStatus`. A file that cannot be opened fails with
        //!                `Io/FileOpenFailed`, a truncated block with `Io/ReadFailed`, and an
        //!                unknown block category with `Format/MalformedData`. Success replaces all blocks;
        //!                failure preserves the previous state. Read blocks can be written again.
        //!
        public:
        GefStatus read ( const std::string& path_in );

        //! \~japanese 上限内で読み込む。失敗・例外時に以前の状態を保持する。
        //! \~english Read within inclusive budgets; failure or exception preserves prior state.
        //! @param [in] path_in Input path.
        //! @param [in] limits_in Input budgets. CSV append counts include retained rows/cells.
        //! @return \~japanese Budget超過はResource/LimitExceeded。 \~english Resource/LimitExceeded on budget exhaustion.
        public: GefStatus readWithLimits(const std::string& path_in, const ReadLimits& limits_in);


        //!
        //! @brief  Write.
        //!
        //! @param [in] path_in       Destination binary file path.
        //! @param [in] is_update_in  Appends to an existing file when true; truncates it when false.
        //!
        //! @return
        //!     \~japanese 成功`GefStatus`。Open失敗は`Io/FileOpenFailed`、途中の読込は`Io/ReadFailed`、書込は`Io/WriteFailed`。
        //!     \~english  The `GefStatus`; open failures use `Io/FileOpenFailed`; late I/O failures use `Io/WriteFailed` for writes and `Io/ReadFailed` for reads.
        //!
        public:
        GefStatus write ( const std::string& path_in, bool is_update_in = false );

        //! \~japanese 同一親Directoryに一時保存し、成功時にFileを置換する。追記・永続化保証はない。
        //! \~english Stage beside the destination and replace on success. No append or crash durability.
        //! @param [in] path_in Destination in a trusted directory; ordinary local files only.
        //! @return \~japanese 失敗時に元Fileを保持する。 \~english Failure preserves the previous destination.
        public: GefStatus writeAtomic(const std::string& path_in);


    };

};
};

#pragma warning(pop)

#endif   //WONDERSTEWENGINE_UTILITYFORMATFILECONTROLLER_BIN_FILECONTROLLER_H

