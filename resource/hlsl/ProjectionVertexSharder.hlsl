//*****************************************************************************************************************
//! 
//! @file    ProjectionVertexSharder.hlsl
//! @brief   File description.
//! @author  
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//! 
//! @date
//!   May-05, 2025   Create New
//!
//!
//! @details  
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
#include"CommonType.hlsli"
BasicType vsMain(float4 pos : POSITION,float2 uv:TEXCOORD) 
{
    BasicType output;//ピクセルシェーダへ渡す値
    output.svpos = pos;
    output.uv = uv;
    return output;
}
