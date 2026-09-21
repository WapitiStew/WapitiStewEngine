//*****************************************************************************************************************
//! 
//! @file    RenderTypes.h
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   2025/05/05    Create new   WapitiStew
//!
//! 
//! @brief   \~japanese Legacy Renderer層が使うFrame記述（解像度・Channel数・Bit深さ）を定義する.
//! @brief   \~english  Defines the frame description (extent, channels, bit depth) the legacy renderer layer uses.
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
#ifndef WONDERSTEWENGINE_OUI_RENDER_UTILITY_RENDERTYPE_H
#define WONDERSTEWENGINE_OUI_RENDER_UTILITY_RENDERTYPE_H

// C/C++
#include <memory>
#include <utility>
#include <vector>

// AppUtil
#include "../../../wse/stew.h"
#include "../../../dynamic.h"

namespace wse
{
namespace oui
{

    //! 
    //! @struct sFrameDesc
    //! 
    //! @brief  
    //!     \~japanese  映像フレームの情報.
    //!     \~english   Video Frame Description
    //! 
    struct sFrameDesc
    {

        //---------------------------------------------------------------------------
        // Member variable
        //---------------------------------------------------------------------------
        wse::int64_size     size;  //!< \~japanese 解像度.       \~english GPU Frame size.
        uint64_t            channels;                  //!< \~japanese チャネル数    \~english image Channels. (RGB->3, RGBA->4)
        uint64_t            bit_depth;                  //!< \~japanese ビット深さ.   \~english bit depth.


        //-----------------------------------------------------------------------------------------
        // Accessor.
        //-----------------------------------------------------------------------------------------
        //!
        //! @brief 
        //!     \~japanese  [Getter] バイト深さを取得する.
        //!     \~english   [Getter] Get Byte depth
        //! 
        //! @return Byte depth.
        //! 
        uint64_t byte_depth( void ) const { return ( ( bit_depth - 1 ) / 8 ) + 1; }

        //!
        //! @brief 
        //!     \~japanese  [Getter] 1行当たりのデータサイズ取得.
        //!     \~english   [Getter] Get Data size par line
        //! 
        //! @return Data size.
        //! 
        uint64_t row_memory_size( void ) const { return static_cast< uint64_t >( this->size.width() ) * this->channels * this->byte_depth(); }
        
        //!
        //! @brief 
        //!     \~japanese  [Getter] メモリサイズ取得.
        //!     \~english   [Getter] Get Data size par line
        //! 
        //! @return Memory size.
        //! 
        uint64_t memory_size( void ) const { return static_cast< uint64_t >( this->size.height() ) * this->row_memory_size(); }


        //---------------------------------------------------------------------------
        // Constructor/Destructor
        //---------------------------------------------------------------------------

        //! 
        //! @brief  Default Constructor.
        //! 
        public :  sFrameDesc(  )
            : size      ()
            , channels  ( 4 )
            , bit_depth ( 8 ){}

        //! 
        //! @brief Destructor.
        //! 
        public : ~sFrameDesc(  ){}

        //! 
        //! @brief Copy constractor.
        //! 
        //! @param [in ] obj_in Copy object
        //! 
        public :  sFrameDesc( const sFrameDesc&  obj_in ) 
            : size      ( obj_in.size )
            , channels  ( obj_in.channels )
            , bit_depth ( obj_in.bit_depth )
        {
        }
        //! 
        //! @brief Move constractor.
        //! 
        //! @param [in,out] obj_inout Move object
        //! 
        public :  sFrameDesc( sFrameDesc&& obj_inout ) noexcept
            : size      ( std::move( obj_inout.size      ) )
            , channels  ( std::move( obj_inout.channels  ) )
            , bit_depth ( std::move( obj_inout.bit_depth ) )
        {
        }
        //!
        //! @brief 
        //!     \~japanese  スワップ
        //!     \~english   swap
        //! 
        //! @param [in,out] obj1_inout Swap object.
        //! @param [in,out] obj2_inout Swap object.
        //! 
        friend void swap( sFrameDesc& obj1_inout, sFrameDesc& obj2_inout )
        {
            std::swap( obj1_inout.size     , obj2_inout.size      );
            std::swap( obj1_inout.channels , obj2_inout.channels  );
            std::swap( obj1_inout.bit_depth, obj2_inout.bit_depth );
        }

        //! 
        //! @brief Copy operator.
        //! 
        //! @param [in ] obj Copy object
        //! 
        public :  sFrameDesc& operator = ( const sFrameDesc&  obj )
        {
            if( this != &obj )
            {
                sFrameDesc src( obj );
                swap( *this, src );
            }
            return *this;
        }

        //! 
        //! @brief Move operator.
        //! 
        //! @param [in ] obj Move object
        //! 
        public :  sFrameDesc& operator = ( sFrameDesc&& obj ) noexcept
        {
            if( this != &obj )
            {
                sFrameDesc src( std::move( obj ) );
                swap( *this, src );
            }
            return *this;
        }
    };
    
    //! 
    //! @brief  
    //!     \~japanese  等号オペレーター
    //!     \~english   equal sign opertor.
    //! 
    //! @param [in ] obj1 Targets for comparison
    //! @param [in ] obj2 Targets for comparison
    //! @retval true  ==
    //! @retval false !=
    //! 
    static bool operator == ( 
            const sFrameDesc& obj1
        ,   const sFrameDesc& obj2
    )
    {
        return ( ( obj1.size      == obj2.size      ) &&
                 ( obj1.channels  == obj2.channels  ) &&
                 ( obj1.bit_depth == obj2.bit_depth ) );
    }
    
    //! 
    //! @brief  
    //!     \~japanese  等号オペレーター
    //!     \~english   inequality sign opertor.
    //! 
    //! @param [in ] obj1 Targets for comparison
    //! @param [in ] obj2 Targets for comparison
    //! @retval true  ==
    //! @retval false !=
    //! 
    static bool operator != ( 
            const sFrameDesc& obj1
        ,   const sFrameDesc& obj2
    )
    {
        return  !( obj1 == obj2 );
    }

};
};


#endif  //WONDERSTEWENGINE_OUI_RENDER_UTILITY_RENDERTYPE_H
