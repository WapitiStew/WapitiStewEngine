//*****************************************************************************************************************
//!
//! @file    ProjectionExample.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Graphics APIへ触れずにRGBA Frameを1枚描画するHandle-free OUI Projection.
//! @brief   \~english  Handle-free OUI projection that renders one RGBA frame without a graphics API.
//!
//! @details
//!     \~japanese
//!     @n 実機不要である。Software Adapterを要求するため、専用GPUが無い環境でも動作する。
//!     @n 引数順序は arguments[0] がNative Library Pathである。追加引数は取らない。
//!     @n Windowは開かない。Projection.Renderは同期1回限りで、Readback済みFrameだけを返す。
//!     \~english
//!     @n Hardware-free: the sample asks for the software adapter, so it runs on machines with no
//!        discrete GPU.
//!     @n Argument order: arguments[0] is the native library path. The sample takes no further
//!        arguments.
//!     @n No window is opened. Projection.Render is one synchronous shot: it builds the renderer,
//!        draws, reads the result back, and tears everything down before it returns. The request
//!        object is therefore the complete description of the render; there is no renderer to
//!        keep, configure, or reuse between calls.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Collections.Generic;
using WapitiStew.Wse;

/// <summary>Handle-free OUI projection rendering.</summary>
public static class ProjectionExample
{
    /// <summary>Renders one projection and prints the resulting frame layout.</summary>
    /// <param name="arguments">Optional path to the native library file.</param>
    /// <returns>Zero on success.</returns>
    public static int Main(string[] arguments)
    {
        if (arguments.Length > 0)
        {
            WseRuntime.SetNativeLibraryPath(arguments[0]);
        }

        using var runtime = new WseRuntime();
        if (!runtime.Info().Components.HasOui)
        {
            Console.Error.WriteLine("This WSE build does not include the OUI component.");
            return 1;
        }

        // A 2x2 RGBA source: red, green, blue, white.
        // Four saturated pixels are the smallest source that still shows where each corner landed,
        // which is what makes the printed top-left pixel a meaningful check. The array is packed
        // RGBA8 and has to be exactly width * height * 4 bytes long.
        byte[] source =
        {
            255, 0, 0, 255,   0, 255, 0, 255,
            0, 0, 255, 255,   255, 255, 255, 255,
        };

        // The mesh maps the source texture onto the output quad in normalized device coordinates.
        // The four vertices are the corners of the whole output: x and y run -1 to 1 with y up,
        // while u and v run 0 to 1 with v down, so v is flipped against y and the source's first
        // pixel arrives at the top left. The six indices are one triangle list, 0-1-2 and 2-1-3,
        // covering that quad; the layer owns no native resource, it is a plain description that
        // Render copies during the call.
        var layer = new ProjectionLayer(
            2,
            2,
            source,
            new List<ProjectionVertex>
            {
                new(-1.0f, 1.0f, 0.0f, 0.0f),
                new(1.0f, 1.0f, 1.0f, 0.0f),
                new(-1.0f, -1.0f, 0.0f, 1.0f),
                new(1.0f, -1.0f, 1.0f, 1.0f),
            },
            new List<int> { 0, 1, 2, 2, 1, 3 })
        {
            // Nearest keeps each source pixel a flat block in the enlarged output, so the printed
            // top-left pixel is exactly the source red. The default, Linear, would blend the four
            // colours across the 4x4 result and give no exact value to compare against.
            SamplingFilter = TextureSamplingFilter.Nearest,
        };

        // A 4x4 output is the smallest size that still magnifies the 2x2 source, and it keeps the
        // printed byte count small enough to read. Opacity, edge blend, clear colour, supersample
        // scale, and the 30-second render deadline are all left at their defaults.
        var request = new ProjectionRequest(4, 4, new List<ProjectionLayer> { layer })
        {
            // Naming the backend keeps the sample deterministic per platform.
            // RendererBackend.Automatic would let OUI choose, which is the usual production choice.
            Backend = OperatingSystem.IsWindows()
                ? RendererBackend.Direct3D12
                : RendererBackend.Vulkan12,
            // Software rendering keeps the sample runnable on machines without a discrete GPU.
            UseSoftwareAdapter = true,
        };

        // Renderer, surface, texture, mesh, and readback objects stay owned inside OUI; only the
        // packed RGBA8 result frame crosses the language boundary.
        // The frame is the one thing the caller owns: it wraps a native buffer, and the using
        // releases it. ToArray copies those bytes into managed memory, so the array below outlives
        // the frame.
        using ProjectionFrame frame = Projection.Render(request);
        byte[] pixels = frame.ToArray();
        Console.WriteLine($"frame size: {frame.Width} x {frame.Height}");
        // Walk rows by the reported row pitch rather than by Width * 4: the pitch is what the
        // backend actually laid the readback out with.
        Console.WriteLine($"row pitch: {frame.RowPitch}");
        Console.WriteLine($"byte count: {pixels.Length}");
        Console.WriteLine($"top-left pixel RGBA: [{pixels[0]}, {pixels[1]}, {pixels[2]}, {pixels[3]}]");
        return 0;
    }
}
