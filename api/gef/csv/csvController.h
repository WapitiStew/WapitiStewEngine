//*****************************************************************************************************************
//! 
//! @file    csvController.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   Arg-30, 2025   Create New     WapitiStew.
//!
//!
//! @brief   \~japanese CSV形式の設定FileをCell表として読み書きするControllerを定義する.
//! @brief   \~english  Defines the controller that reads and writes CSV setting files as a cell table.
//!
//*****************************************************************************************************************
#ifndef WONDERSTEWENGINE_UTILITYFORMATFILECONTROLLER_CSV_FILECONTROLLER_H
#define WONDERSTEWENGINE_UTILITYFORMATFILECONTROLLER_CSV_FILECONTROLLER_H


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
    class WSE_API CSVController
    {

        //----------------------------------
        // Member
        //----------------------------------
        private: std::vector< std::vector< std::string > > m_contents;    //!< CSV全体のCell表（行×列）.

        //----------------------------------
        // Accessor
        //----------------------------------
        public: const std::vector< std::vector< std::string > >& contents( void )const { return this->m_contents; };


        //----------------------------------
        // Constractor / Destructor
        //----------------------------------

        //!
        //! @brief  Default constructor.
        //!
        public: CSVController();

        //!
        //! @brief  Destructor.
        //!
        public: ~CSVController();

        //! 
        //! @brief  Copy constructor.
        //!
        //! @param [in ] obj_in Source object.
        //! 
        public: CSVController( const CSVController& obj_in )
            : m_contents ( obj_in.m_contents ){}

        //! 
        //! @brief  Move constructor.
        //!
        //! @param [in,out] obj_inout Source object.
        //! 
        public:CSVController( CSVController&& obj_inout )noexcept
            : m_contents ( std::move( obj_inout.m_contents ) ){}

        //! 
        //! @brief
        //!     \~japanese スワップ処理。
        //!     \~english Swap two objects.
        //!
        //! @param [in,out] obj1_inout Swap object.
        //! @param [in,out] obj2_inout Swap object.
        //! 
        friend void swap(CSVController& obj1_inout,CSVController& obj2_inout ) noexcept
        {
            std::swap(obj1_inout.m_contents, obj2_inout.m_contents);
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
        CSVController& operator = ( const CSVController& obj )
        {
            CSVController copy_obj(obj);
            swap(*this, copy_obj);
            return *this;
        }

        //! 
        //! @brief  Move assignment operator.
        //!
        //! @param [in ] obj Source object.
        //! 
        public:
        CSVController& operator = ( CSVController&& obj )noexcept
        {
            CSVController move_obj(std::move(obj));
            swap(*this, move_obj);
            return *this;
        }


        //--------------------------------------------------------------------------------
        // Specific Method
        //--------------------------------------------------------------------------------

        //!
        //! @brief \~japanese 全入力行を追記する。失敗時は元の表を保持する。
        //!        \~english Append all input rows; preserve the old table on failure.
        //!
        //! @param [in ] path_in
        //!
        //! @return
        //!     \~japanese 成功`GefStatus`。Open失敗は`Io/FileOpenFailed`、途中の読込は`Io/ReadFailed`、書込は`Io/WriteFailed`。
        //!     \~english  The `GefStatus`; open failures use `Io/FileOpenFailed`; late I/O failures use `Io/WriteFailed` for writes and `Io/ReadFailed` for reads.
        //!
        public:
        GefStatus read ( const std::string& path_in );

        //! \~japanese 上限内で読み込む。失敗・例外時に以前の状態を保持する。
        //! \~english Read within inclusive budgets; failure or exception preserves prior state.
        //! @param [in] path_in Input path.
        //! @param [in] limits_in Input budgets. CSV append counts include retained rows/cells.
        //! @return \~japanese Budget超過はResource/LimitExceeded。 \~english Resource/LimitExceeded on budget exhaustion.
        public: GefStatus readWithLimits(const std::string& path_in, const ReadLimits& limits_in);

        //! \~japanese 上限付き読込で表を置換する。従来readは行を追記する。
        //! \~english Replace the table with a bounded read; legacy read appends rows.
        //! @param [in] path_in Input path.
        //! @param [in] limits_in Inclusive budgets, using bounded defaults when omitted.
        public: GefStatus readReplace(const std::string& path_in, const ReadLimits& limits_in = ReadLimits{});


        //!
        //! @brief  Write.
        //!
        //! @param [in] path_in  Destination CSV file path.
        //!
        //! @return
        //!     \~japanese 成功`GefStatus`。Open失敗は`Io/FileOpenFailed`、途中の読込は`Io/ReadFailed`、書込は`Io/WriteFailed`。
        //!     \~english  The `GefStatus`; open failures use `Io/FileOpenFailed`; late I/O failures use `Io/WriteFailed` for writes and `Io/ReadFailed` for reads.
        //!
        public:
        GefStatus write ( const std::string& path_in );

        //! \~japanese 同一親Directoryに一時保存し、成功時にFileを置換する。追記・永続化保証はない。
        //! \~english Stage beside the destination and replace on success. No append or crash durability.
        //! @param [in] path_in Destination in a trusted directory; ordinary local files only.
        //! @return \~japanese 失敗時に元Fileを保持する。 \~english Failure preserves the previous destination.
        public: GefStatus writeAtomic(const std::string& path_in);


        //! 
        //! @brief  toDataFrameMap.
        //! 
        //! @details
        //! 
        //! Labal1| Labal2   |Labal3  |Labal4
        //! ------+----------+--------+--------
        //! Key1  | Category1|Param1 |Remark1
        //! Key2  | Category2|Param2 |Remark2
        //! Key3  | Category3|Param3 |Remark3
        //! Key4  | Category4|Param4 |Remark4
        //!
        //! @return
        //!     \~japanese DataMapを運ぶ`GefResult`. Data行が無い表は`Format/NoData`,
        //!                4 Cell未満のLabel行・5 Cell未満のData行は`Format/MalformedData`で失敗する.
        //!     \~english  The `GefResult` carrying the data map. A table without data rows fails
        //!                with `Format/NoData`; a label row with fewer than four cells or data row with fewer than five fails with
        //!                `Format/MalformedData`.
        //!
        public: GefResult< datamap_2d > toDataMAP2D ( void );

    };

};
};

#pragma warning(pop)

#endif   //WONDERSTEWENGINE_UTILITYFORMATFILECONTROLLER_CSV_FILECONTROLLER_H

