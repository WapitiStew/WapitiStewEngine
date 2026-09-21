//*****************************************************************************************************************
//! 
//! @file    Parameter.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @date
//!   Feb-17, 2025   Create New     WapitiStew.
//!
//!
//! @brief
//!     \~japanese カメラ制御用の汎用テンプレートパラメータクラスの定義
//!     \~english  Definition of a generic template parameter class for camera control
//!
//! 
//! @details
//!     \~japanese
//!     @n 本ファイルでは、カメラ制御に用いるパラメータ情報を保持する `wse::tmr::Parameter` テンプレートクラスを定義する。 
//!     @n 本クラスは制御方式の種別（手動・自動・自動調整）やパラメータ型（数値／リスト）に対応し、 
//!     @n パラメータの初期値、現在値、分解能、設定可能範囲などの情報を包括的に管理する。
//!     @n 対応状況をビットフィールドで記録し、柔軟なアクセス・設定機構を提供する。
//!
//!     \~english
//!     @n This file defines the `wse::tmr::Parameter` template class used for camera parameter control.
//!     @n The class supports multiple control modes (manual, auto, auto-adjust) and parameter types (value/list).
//!     @n It comprehensively manages parameter state such as initial/current value, resolution, and valid range.
//!     @n Control mode support is tracked using a bitfield, and flexible access/setter functions are provided.
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
#ifndef WONDERSTEWENGINE_TURTLECAMERALIB_DATA_PARAMETER_H
#define WONDERSTEWENGINE_TURTLECAMERALIB_DATA_PARAMETER_H

#pragma warning(push)
#pragma warning(disable: 4464)  // ../
#include <stdexcept>
#include "../../wse/stew.h"
#include "../depend/STD.h"
#include "../device/WebCameraCompatibility.h"
#include "../../dynamic.h"
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 4251)
#pragma warning(disable: 4820)  // 'wse::tmr::Parameter<long>': '1' バイトのパディングを データ メンバー 'wse::tmr::Parameter<long>::m_supported_control_bitfield' の後に追加しました。
namespace wse
{
namespace tmr
{

    //! 
    //! @class  eParamType
    //! @brief 
    //!     \~japanese  パラメータの種類.
    //!     \~english   Parameter types.
    //! 
    enum class WSE_API eParamType : int16_t
    {
            VALUE = 0   //!< \~japanese 数値パラメータ                 \~english Value parameters
        ,   LIST  = 1   //!< \~japanese リスト化されたパラメーター.    \~english Listed parameters.
    };
        
    
    //! 
    //! @class  Parameter
    //! 
    //! @brief
    //!     \~japanese カメラの制御パラメータ情報を保持・操作するテンプレートクラス
    //!     \~english  Templated class for storing and controlling device parameters
    //!
    //! @details
    //!     \~japanese
    //!         このクラスは、型 `_Tp` に基づいてデバイスの制御パラメータを保持・管理する。
    //!         各パラメータには、制御方式のサポート状況（手動・自動・自動調整）、初期値、現在値、
    //!         分解能（ステップ）、および設定可能値のリストなどの情報が含まれる。
    //!         パラメータ型（連続値 or リスト）により扱いが異なる。
    //!
    //!     \~english
    //!         This class stores and manages device control parameters based on the template type `_Tp`.
    //!         Each parameter includes support status for control modes (manual, auto, auto-adjust),
    //!         initial value, current value, resolution (step), and a list of valid values.
    //!         The behavior differs depending on whether the parameter type is continuous or list-based.
    //!
    //! @par
    //!     \~japanese 主な機能
    //!     - テンプレート型 `_Tp` により値の型を柔軟に指定可能（int, float など）
    //!     - 制御モードのサポート状況をビットフィールドで管理
    //!     - `eParamType::VALUE` の場合、最小値と最大値は `m_settable_value_list` の先頭・末尾に格納
    //!     - `logout()` 関数で設定内容をログ出力可能
    //!     - `Getter/Setter` を使って柔軟に値へアクセスできる
    //!
    //!     \~english Key Features
    //!     - Template parameter `_Tp` allows flexible value type (e.g., int, float)
    //!     - Support flags for control modes are managed as a bitfield
    //!     - For `eParamType::VALUE`, min and max values are taken from the front and back of `m_settable_value_list`
    //!     - `logout()` method outputs the current configuration to the debug log
    //!     - Getter and setter methods provide flexible access to parameter data
    //!
    //! @tparam _Tp
    //!     \~japanese パラメータの値型（int や float など）
    //!     \~english  Value type for the parameter (e.g., int, float)
    //! 
    template < typename _Tp >
    class WSE_API Parameter
    {
        //------------------------------------------------------------------------------------------
        // Member variables
        //------------------------------------------------------------------------------------------
        private : uint8_t       m_supported_control_bitfield  ;  //!< \~english Determine if the device supports parameter control. \~japanese デバイスがパラメータ制御サポート判定.
        private : eParamType    m_type                        ;  //!< \~english Parameter type.                                     \~japanese パラメータの種類.
        private : eControlMode  m_mode                        ;  //!< \~english How to set the value.                               \~japanese 値の設定方法.
        private : _Tp           m_init_value                  ;  //!< \~english Initial value of the device (for manual).           \~japanese デバイスの初期値  (マニュアル用).
        private : _Tp           m_step                        ;  //!< \~english Resolution (for manual).                            \~japanese 分解能            (マニュアル用).
        private : _Tp           m_current_value               ;  //!< \~english Setting value (for manual).                         \~japanese 設定している値.   (マニュアル用)
        private : std::vector< _Tp > m_settable_value_list    ;  //!< \~english Settable value (for manual).                        \~japanese 設定可能な値.     (マニュアル用)

        //!
        //! @brief 
        //!     \~japanese  スワップ
        //!     \~english   swap
        //! 
        //! @param [in,out] obj1_inout Swap object.
        //! @param [in,out] obj2_inout Swap object.
        //! 
        friend void swap( Parameter &obj1_inout, Parameter &obj2_inout )
        {
            std::swap( obj1_inout.m_supported_control_bitfield , obj2_inout.m_supported_control_bitfield );
            std::swap( obj1_inout.m_type                       , obj2_inout.m_type                       );
            std::swap( obj1_inout.m_mode                       , obj2_inout.m_mode                       );
            std::swap( obj1_inout.m_init_value                 , obj2_inout.m_init_value                 );
            std::swap( obj1_inout.m_step                       , obj2_inout.m_step                       );
            std::swap( obj1_inout.m_current_value              , obj2_inout.m_current_value              );
            std::swap( obj1_inout.m_settable_value_list        , obj2_inout.m_settable_value_list        );
        }


        //------------------------------------------------------------------------------------------
        // Getter
        //------------------------------------------------------------------------------------------

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 対応している制御方式なのか判定する.
        //!     \~english   [Getter] Judge whether the control method is supported.
        //! 
        //! @param [in] mode_in    Mode bitfield
        //! @param [in] target_in  Judgment subject
        //! 
        //! @retval true    Supported.
        //! @retval false   Unsupported.
        //! 
        private: static bool isSupport( const uint8_t mode_in, const eControlMode target_in )
        {
            return ( ( mode_in & static_cast< uint8_t >( target_in ) ) > 0 );
        }

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 対応している制御方式なのか判定する.
        //!     \~english   [Getter] Judge whether the control method is supported.
        //! 
        //! @param [in] mode_in    Judgment mode
        //! 
        //! @retval true    Supported.
        //! @retval false   Unsupported.
        //! 
        public : bool isSupport( const eControlMode mode_in )const 
        { 
            return isSupport( this->m_supported_control_bitfield, mode_in );
        }

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 制御可能なパラメータなのかを判定する.
        //!     \~english   [Getter] Judge if it is a controllable parameter.
        //! 
        //! @retval true    Controlable.
        //! @retval false   Uncontrollable.
        //! 
        public : bool isSupport( void )const 
        { 
            return isSupport( eControlMode::MANUAL      ) ||
                   isSupport( eControlMode::AUTO        ) ||
                   isSupport( eControlMode::AUTO_ADJUST );
        }

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 対応可能制御方式のビットフィールドを取得する.
        //!     \~english   [Getter] Gets the bitfield of supported control methods.
        //! 
        //! @return bitfield.
        //! 
        public : uint8_t supported_control_bitfield( void )const{ return this->m_supported_control_bitfield; } 

        //! 
        //! @brief 
        //!     \~japanese  [Getter] パラメータ種類を取得する.
        //!     \~english   [Getter] Gets parameter type.
        //! 
        //! @return type.
        //! 
        public : eParamType type( void )const{ return this->m_type; } 

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 今の制御方式を取得する.
        //!     \~english   [Getter] Gets current cotrol mode.
        //! 
        //! @return control mode.
        //! 
        public : eControlMode mode( void )const{ return this->m_mode ; } 

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 初期値を取得する.
        //!     \~english   [Getter] Gets Initial Value.
        //! 
        //! @return Initial Value.
        //! 
        public : _Tp init_value( void )const{ return this->m_init_value; } 

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 制御可能な分解能を取得する.
        //!     \~english   [Getter] Gets Controllable step.
        //! 
        //! @return step.
        //! 
        public : _Tp step ( void )const{ return this->m_step; } 

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 現在値を取得する.
        //!     \~english   [Getter] Gets current value.
        //! 
        //! @return current value.
        //! 
        public : _Tp current_value( void )const{ return this->m_current_value; }

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 設定可能な値のリストを取得する. ( for eParamType::LIST用 ) 
        //!     \~english   [Getter] Gets settable value list.       ( for eParamType::LIST ) 
        //! 
        //! @return type.
        //! 
        public: std::vector< _Tp > settable_value_list( void )const{ return this->m_settable_value_list; }

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 最大値を取得する.( eParamType::VALUE用 )
        //!     \~english   [Getter] Gets maximum value.( for eParamType::VALUE ) 
        //! 
        //! @return maximum value.
        //! 
        public : _Tp max_value( void ) const
        { 
            if( this->m_type == eParamType::LIST )
            {
                throw std::invalid_argument( "the camera parameter value is not supported" );
            }

            return this->m_settable_value_list.back();
        }

        //! 
        //! @brief 
        //!     \~japanese  [Getter] 最小値を取得する.( eParamType::VALUE用 )
        //!     \~english   [Getter] Gets minimum value.( for eParamType::VALUE ) 
        //! 
        //! @return minimum value.
        //! 
        public : _Tp min_value( void ) const
        { 
            return this->m_settable_value_list.front();
        }


        //------------------------------------------------------------------------------------------
        // Setter
        //------------------------------------------------------------------------------------------

        //! 
        //! @brief 
        //!     \~japanese  [Setter] 制御方式対応情報を設定する.
        //!     \~english   [Setter] Sets supported info of control mode
        //! 
        //! @param [in ] mode_in       Control mode
        //! @param [in ] supported_in  Supported info ( true = Supported, false = Unsupported )
        //! 
        public : void setControlMode( const eControlMode mode_in, const bool supported_in )
        { 
            if( supported_in )
            {
                this->m_supported_control_bitfield |= static_cast< uint8_t >( mode_in );
            }
            else
            {
                this->m_supported_control_bitfield &= ~static_cast< uint8_t >( mode_in );
            }
        }

        //! 
        //! @brief 
        //!     \~japanese  [Setter] 初期値を設定する.
        //!     \~english   [Setter] Sets Initial Value.
        //! 
        //! @param [in ] value_in       Initial mode
        //! 
        public : void setInitialValue( const _Tp& value_in ) { this->m_init_value = value_in; }

        //! 
        //! @brief 
        //!     \~japanese  [Setter] パラメータ種別を設定する.
        //!     \~english   [Setter] Sets Parameter type.
        //! 
        //! @param [in ] type_in       parameter type
        //! 
        public : void setType( const eParamType& type_in ) { this->m_type = type_in; }

        //! 
        //! @brief 
        //!     \~japanese  [Setter] 分解能を設定する. ( eParamType::VALUE用 )
        //!     \~english   [Setter] Sets Parameter step( for eParamType::VALUE ).
        //! 
        //! @param [in ] step_in       parameter type
        //! 
        public : void setStep( const _Tp& step_in ) { this->m_step = step_in; }

        //! 
        //! @brief 
        //!     \~japanese  [Setter] 現在値を設定する. ( eParamType::VALUE用 )
        //!     \~english   [Setter] Sets Current value( for eParamType::VALUE ).
        //! 
        //! @param [in ] value_in       parameter value
        //! 
        public : void setCurrentValue( const _Tp& value_in ) { this->m_current_value = value_in; }

        //! 
        //! @brief 
        //!     \~japanese  [Setter] 設定可能な数値リストを設定する. 
        //!     \~english   [Setter] Sets Settable value list
        //! 
        //! @details
        //!     \~japanese  
        //!         eParamType::VALUEの場合は最初の要素に最小値として、一番後ろの要素を最大値として扱う
        //!     \~english   
        //!         In the case of eParamType::VALUE, the first element is treated as the minimum value and the last element is treated as the maximum value.
        //! 
        //! 
        //! @param [in ] value_in       parameter value list
        //! 
        public : void setSettableValueList ( const std::vector< _Tp >& value_in ) { this->m_settable_value_list = value_in; }


        //------------------------------------------------------------------------------------------
        // Constractor / Destractor
        //------------------------------------------------------------------------------------------

        //! 
        //! @brief  Default constoractor.
        //! 
        public : Parameter( void )
            : m_supported_control_bitfield ( 0 )
            , m_type                       ( eParamType::VALUE )
            , m_mode                       ( eControlMode::MANUAL )
            , m_init_value                 ( 0 )
            , m_step                       ( 0 )
            , m_current_value              ( 0 )
            , m_settable_value_list        ( { 0, 0 } )
        {
        }
        //! 
        //! @brief  Constoractor.
        //! 
        //! @param [in ] is_supported_manual_in       Manual support information
        //! @param [in ] is_supported_auto_in         Automatic adjustment support information
        //! @param [in ] is_supported_auto_adjust_in  Automatic adjustment support information
        //! @param [in ] type_in                      Parameter type.
        //! @param [in ] mode_in                      Control method
        //! @param [in ] init_value_in                Initial value
        //! @param [in ] step_in                      Resolution
        //! @param [in ] current_value_in             Current value
        //! @param [in ] value_list_in                Settable numerical information (when eParamType::VALUE, the first element should be the minimum value and the last element should be the maximum value)
        //! 
        public : Parameter( 
                const bool                  is_supported_manual_in
            ,   const bool                  is_supported_auto_in
            ,   const bool                  is_supported_auto_adjust_in
            ,   const eParamType            type_in
            ,   const eControlMode          mode_in                     = eControlMode::MANUAL
            ,   const _Tp                   init_value_in               = static_cast< _Tp >( 0 )
            ,   const _Tp                   step_in                     = static_cast< _Tp >( 0 )
            ,   const _Tp                   current_value_in            = static_cast< _Tp >( 0 )
            ,   const std::vector< _Tp >&   value_list_in               = std::vector< _Tp >( 0 )
        )
            : m_supported_control_bitfield ( 0 )
            , m_type                       ( type_in )
            , m_mode                       ( mode_in )
            , m_init_value                 ( init_value_in )
            , m_step                       ( step_in )
            , m_current_value              ( current_value_in )
            , m_settable_value_list        ( value_list_in )
        {
            setControlMode( eControlMode::MANUAL     , is_supported_manual_in );
            setControlMode( eControlMode::AUTO       , is_supported_auto_in );
            setControlMode( eControlMode::AUTO_ADJUST, is_supported_auto_adjust_in );
        }

        //! 
        //! @brief  Destructor.
        //! 
        public: ~Parameter( void ){};

        //! 
        //! @brief Copy constoractor.
        //! @param [in] obj_in Copy src object.
        //! 
        Parameter(const Parameter &obj_in)
            : m_supported_control_bitfield ( obj_in.m_supported_control_bitfield )
            , m_type                       ( obj_in.m_type )
            , m_mode                       ( obj_in.m_mode )
            , m_init_value                 ( obj_in.m_init_value )
            , m_step                       ( obj_in.m_step )
            , m_current_value              ( obj_in.m_current_value )
            , m_settable_value_list        ( obj_in.m_settable_value_list )
        {
        }

        //! 
        //! @brief Move constoractor.
        //! @param [in,out] obj_inout Muve src object.
        //! 
        Parameter(Parameter &&obj_inout) noexcept
            : m_supported_control_bitfield ( std::move( obj_inout.m_supported_control_bitfield ) )
            , m_type                       ( std::move( obj_inout.m_type                       ) )
            , m_mode                       ( std::move( obj_inout.m_mode                       ) )
            , m_init_value                 ( std::move( obj_inout.m_init_value                 ) )
            , m_step                       ( std::move( obj_inout.m_step                       ) )
            , m_current_value              ( std::move( obj_inout.m_current_value              ) )
            , m_settable_value_list        ( std::move( obj_inout.m_settable_value_list        ) )
        {
        }

        //! 
        //! @brief Copy operator.
        //! 
        //! @param [in ] obj Copy object
        //! 
        Parameter &operator=(const Parameter &obj)
        {
            if( this != &obj )
            {
                Parameter tmp( obj );
                swap( *this, tmp );
            }
            return *this;
        }

        //! 
        //! @brief Move operator.
        //! 
        //! @param [in ] obj Move object
        //! 
        Parameter &operator=(Parameter &&obj) noexcept
        {
            if( this != &obj )
            {
                Parameter tmp( std::move( obj ) );
                swap( *this, tmp );
            }
            return *this;
        }

#pragma warning(push)
#pragma warning(disable: 5045)  // ../
        public : void logout( const std::string &tag_in, const size_t max_id_in, const size_t min_id_in )
        { 
            if( !this->isSupport() )
            { 
                DDLog() << tag_in << " : UNSUPPORTED DEVICE";
                return; 
            }            
            DDLog() << tag_in << " : SUPPORTED DEVICE";
            std::string supported_info = tag_in + " SUPPORTED INFOMATION ->> "
                                       + "Manual : "     + ( ( isSupport( eControlMode::MANUAL      ) ) ? "SUPPORTED" : "UNSUPPORTED" ) + "  /  "
                                       + "Auto : "       + ( ( isSupport( eControlMode::AUTO        ) ) ? "SUPPORTED" : "UNSUPPORTED" ) + "  /  "
                                       + "AutoAdjust : " + ( ( isSupport( eControlMode::AUTO_ADJUST ) ) ? "SUPPORTED" : "UNSUPPORTED" );
            DDLog() << supported_info;
            
            
            std::string value_info = ( eParamType::VALUE == this->m_type ) ? "Value" :"List";
            DDLog() << tag_in << " PROPATY INFORMATION  ->> TYPE : " + value_info;
            if( eParamType::VALUE == this->m_type )
            {
                std::string str = tag_in + " VALUE INFOMATION ->> "
                                + "Min : "  + std::to_string( this->m_settable_value_list[ min_id_in ] ) + "  /  "
                                + "Max : "  + std::to_string( this->m_settable_value_list[ max_id_in ] ) + "  /  "
                                + "Step : " + std::to_string( this->m_step );
                DDLog() << str;
            }
            else if( eParamType::LIST == this->m_type )
            {
                std::string str = tag_in + " LIST INFOMATION ->> "
                                + "Size : " + std::to_string( this->m_settable_value_list.size() );
                std::string list_str = tag_in + "  LIST ITEMS ->>";
                for( size_t i = 0; i < this->m_settable_value_list.size(); ++i )
                {
                    list_str += "ID = " + std::to_string( i ) + " -> " + std::to_string( this->m_settable_value_list[ i ] ) +"  /  " ;
                }
                DDLog() << list_str;
            }
            std::string current_info = tag_in + " CURRENT PROPATY INFOMATION ->>"
                                     + "  ControlMode :" + ( isSupport( eControlMode::MANUAL      ) ? "MANUAL" : 
                                                           ( isSupport( eControlMode::AUTO      ) ? "AUTO" : "AUTO_ADJUST" ) );
            DDLog() << current_info;
        }
#pragma warning(pop)

    };

}
}
#pragma warning(pop)

#endif //WONDERSTEWENGINE_TURTLECAMERALIB_DATA_PARAMETER_H
