//*****************************************************************************************************************
//! 
//! @file    SettingFormat.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Arg-30, 2025   Create New     WapitiStew.
//!
//!
//! @brief   \~japanese CSV設定Fileの列名・Category文字列と、その型Categoryへの変換を定義する.
//! @brief   \~english  Defines the CSV setting-file column names, category strings, and their conversion
//!                     to the typed category.
//!
//*****************************************************************************************************************
#ifndef WONDERSTEWENGINE_UTILITYFORMATFILECONTROLLER_FORMAT_ENUM_H
#define WONDERSTEWENGINE_UTILITYFORMATFILECONTROLLER_FORMAT_ENUM_H

#include <cstdint>
#include <string>
#include <map>

#pragma warning(push)
#pragma warning(disable: 4505)  // 内部リンケージ含む参照されていない関数の警告.
namespace wse
{
namespace gef
{
    static const std::string SETTING_KEY    = "Key";
    static const std::string SETTING_CAT    = "Category";
    static const std::string SETTING_REMARK = "Remark";
    static const std::string SETTING_Num    = "Num";
    static const size_t      SETTING_HEADER = 4;

    static const std::string SETTING_PARAM  = "Param";
    static const std::string getParamKey( const uint8_t index_in )
    {
        return SETTING_PARAM + std::to_string( index_in );
    }



    
    static const std::string SETTING_CAT_Enum         = "enum";
    static const std::string SETTING_CAT_Integer08    = "int08";
    static const std::string SETTING_CAT_Integer16    = "int16";
    static const std::string SETTING_CAT_Integer32    = "int32";
    static const std::string SETTING_CAT_Integer64    = "int64";
    static const std::string SETTING_CAT_Float32      = "float32";
    static const std::string SETTING_CAT_Float64      = "float64";
    enum class eCategory : uint8_t
    {
          None      = 0
        , Enum      = 1     //!< SETTING_CAT_Enum     .
        , Integer08 = 2     //!< SETTING_CAT_Integer08.
        , Integer16 = 3     //!< SETTING_CAT_Integer16.
        , Integer32 = 4     //!< SETTING_CAT_Integer32.
        , Integer64 = 5     //!< SETTING_CAT_Integer64.
        , Float32   = 6     //!< SETTING_CAT_Float32  .
        , Float64   = 7     //!< SETTING_CAT_Float64  .
    };

    static eCategory convertCategory( const std::string& value_in )
    {
             if( value_in == SETTING_CAT_Enum      ) { return eCategory::Enum      ; }
        else if( value_in == SETTING_CAT_Integer08 ) { return eCategory::Integer08 ; }
        else if( value_in == SETTING_CAT_Integer16 ) { return eCategory::Integer16 ; }
        else if( value_in == SETTING_CAT_Integer32 ) { return eCategory::Integer32 ; }
        else if( value_in == SETTING_CAT_Integer64 ) { return eCategory::Integer64 ; }
        else if( value_in == SETTING_CAT_Float32   ) { return eCategory::Float32   ; }
        else if( value_in == SETTING_CAT_Float64   ) { return eCategory::Float64   ; }
        return eCategory::None;
    }
    typedef std::map<std::string, std::map<std::string, std::vector< std::string >>> datamap_2d;


};
};

#pragma warning(pop)
#pragma warning(disable: 4505)  // 内部リンケージ含む参照されていない関数の警告.
#endif   //WONDERSTEWENGINE_VIKINGPROJECTORLIB_DEPEND_ENUM_H

