//*****************************************************************************************************************
//! 
//! @file    ProjectionPixelSharder_Downsampling.hlsl
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
Texture2D<float4> tex_contents : register(t0); //0番スロットに設定されたテクスチャ
SamplerState smp : register(s0); //0番スロットに設定されたサンプラ


float4 psMain( BasicType input ) : SV_TARGET
{
    // 1) カラーサンプリング
    float4 color = tex_contents.Sample( smp, input.uv );
    // 3) 出力に合成
    return float4( color.r, color.g,color.b,color.a ).rgba;  //float4( color.rgb, alpha);

}