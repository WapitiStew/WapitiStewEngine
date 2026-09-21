//*****************************************************************************************************************
//! 
//! @file    WindowPixelShader.hlsl
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
Texture2D<float4> tex:register(t0);//0番スロットに設定されたテクスチャ
SamplerState smp:register(s0);//0番スロットに設定されたサンプラ


float4 BasicPS(BasicType input ) : SV_TARGET
{
    return float4(tex.Sample(smp,input.uv)).bgra;
}