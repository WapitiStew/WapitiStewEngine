//*****************************************************************************************************************
//!
//! @file    Projection.cs
//! @par        Character Code: UTF-8N
//! @par        Return Code: LF
//!
//! @brief   \~japanese Handle-freeなOUI Projection描画の公開入口.
//! @brief   \~english  Public entry point for handle-free OUI projection rendering.
//!
//! @details
//!     \~japanese
//!     @n Renderer、Surface、Texture、Mesh、FenceおよびReadback ObjectはOUI内部の所有のままで、
//!        返却するpacked RGBA8 Frameだけが言語境界を越える。GPU HandleもFenceも公開しない。
//!     \~english
//!     @n Renderer, surface, texture, mesh, fence, and readback objects stay owned inside OUI; only
//!        the packed RGBA8 result frame crosses the language boundary. No GPU handle or fence leaks.
//!
//! @date
//!   Sep-01, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace WapitiStew.Wse;

/// <summary>Graphics backend selection for a projection render.</summary>
public enum RendererBackend
{
    /// <summary>Let OUI pick the backend supported by this platform.</summary>
    Automatic = 0,

    /// <summary>Direct3D 12, available on Windows.</summary>
    Direct3D12 = 1,

    /// <summary>Vulkan 1.2, available on Linux.</summary>
    Vulkan12 = 2,
}

/// <summary>Texture sampling filter used when a layer is warped onto the output.</summary>
public enum TextureSamplingFilter
{
    /// <summary>Nearest-neighbour sampling.</summary>
    Nearest = 0,

    /// <summary>Bilinear sampling.</summary>
    Linear = 1,
}

/// <summary>Falloff curve applied across an edge-blend band.</summary>
public enum EdgeBlendCurve
{
    /// <summary>Linear falloff.</summary>
    Linear = 0,

    /// <summary>Smoothstep falloff.</summary>
    Smoothstep = 1,
}

/// <summary>One projection vertex: position in normalized device coordinates and a texture UV.</summary>
/// <param name="X">Horizontal position in normalized device coordinates.</param>
/// <param name="Y">Vertical position in normalized device coordinates.</param>
/// <param name="U">Horizontal texture coordinate.</param>
/// <param name="V">Vertical texture coordinate.</param>
public readonly record struct ProjectionVertex(float X, float Y, float U, float V);

/// <summary>Linear RGBA clear color.</summary>
/// <param name="Red">Red component.</param>
/// <param name="Green">Green component.</param>
/// <param name="Blue">Blue component.</param>
/// <param name="Alpha">Alpha component.</param>
public readonly record struct RendererColor(float Red, float Green, float Blue, float Alpha)
{
    /// <summary>Gets an opaque black clear color.</summary>
    public static RendererColor OpaqueBlack => new(0.0f, 0.0f, 0.0f, 1.0f);
}

/// <summary>Edge-blend widths measured from the texture UV borders.</summary>
/// <param name="Left">Normalized U width from the left edge.</param>
/// <param name="Right">Normalized U width from the right edge.</param>
/// <param name="Top">Normalized V width from the top edge.</param>
/// <param name="Bottom">Normalized V width from the bottom edge.</param>
/// <param name="Curve">Falloff curve applied across each band.</param>
public readonly record struct EdgeBlend(
    float Left,
    float Right,
    float Top,
    float Bottom,
    EdgeBlendCurve Curve)
{
    /// <summary>Gets an edge blend that leaves every edge fully opaque.</summary>
    public static EdgeBlend None => new(0.0f, 0.0f, 0.0f, 0.0f, EdgeBlendCurve.Linear);
}

/// <summary>One RGBA source image warped onto the output by its own mesh.</summary>
public sealed class ProjectionLayer
{
    /// <summary>Creates a layer from an RGBA source and its mesh.</summary>
    /// <param name="width">Source width in pixels.</param>
    /// <param name="height">Source height in pixels.</param>
    /// <param name="rgba">Packed RGBA8 source bytes, <c>width * height * 4</c> long.</param>
    /// <param name="vertices">Mesh vertices.</param>
    /// <param name="indices">Triangle-list indices into <paramref name="vertices"/>.</param>
    public ProjectionLayer(
        int width,
        int height,
        byte[] rgba,
        IReadOnlyList<ProjectionVertex> vertices,
        IReadOnlyList<int> indices)
    {
        ArgumentNullException.ThrowIfNull(rgba);
        ArgumentNullException.ThrowIfNull(vertices);
        ArgumentNullException.ThrowIfNull(indices);

        Width = width;
        Height = height;
        Rgba = rgba;
        Vertices = vertices;
        Indices = indices;
    }

    /// <summary>Gets the source width in pixels.</summary>
    public int Width { get; }

    /// <summary>Gets the source height in pixels.</summary>
    public int Height { get; }

    /// <summary>Gets the packed RGBA8 source bytes.</summary>
    public byte[] Rgba { get; }

    /// <summary>Gets or sets the optional packed R8 alpha map.</summary>
    public byte[]? Alpha { get; set; }

    /// <summary>Gets the mesh vertices.</summary>
    public IReadOnlyList<ProjectionVertex> Vertices { get; }

    /// <summary>Gets the triangle-list indices.</summary>
    public IReadOnlyList<int> Indices { get; }

    /// <summary>Gets or sets the sampling filter. Defaults to <see cref="TextureSamplingFilter.Linear"/>.</summary>
    public TextureSamplingFilter SamplingFilter { get; set; } = TextureSamplingFilter.Linear;

    /// <summary>Gets or sets the layer opacity. Defaults to fully opaque.</summary>
    public float Opacity { get; set; } = 1.0f;

    /// <summary>Gets or sets the edge blend. Defaults to <see cref="EdgeBlend.None"/>.</summary>
    public EdgeBlend EdgeBlend { get; set; } = EdgeBlend.None;
}

/// <summary>A complete projection render request.</summary>
public sealed class ProjectionRequest
{
    /// <summary>Creates a request for a given output size and layer set.</summary>
    /// <param name="outputWidth">Output width in pixels.</param>
    /// <param name="outputHeight">Output height in pixels.</param>
    /// <param name="layers">Layers rendered in order.</param>
    public ProjectionRequest(int outputWidth, int outputHeight, IReadOnlyList<ProjectionLayer> layers)
    {
        ArgumentNullException.ThrowIfNull(layers);
        OutputWidth = outputWidth;
        OutputHeight = outputHeight;
        Layers = layers;
    }

    /// <summary>Gets the output width in pixels.</summary>
    public int OutputWidth { get; }

    /// <summary>Gets the output height in pixels.</summary>
    public int OutputHeight { get; }

    /// <summary>Gets the layers rendered in order.</summary>
    public IReadOnlyList<ProjectionLayer> Layers { get; }

    /// <summary>Gets or sets the clear color. Defaults to opaque black.</summary>
    public RendererColor ClearColor { get; set; } = RendererColor.OpaqueBlack;

    /// <summary>Gets or sets the supersample scale. Defaults to one.</summary>
    public int SupersampleScale { get; set; } = 1;

    /// <summary>Gets or sets the backend. Defaults to <see cref="RendererBackend.Automatic"/>.</summary>
    public RendererBackend Backend { get; set; } = RendererBackend.Automatic;

    /// <summary>
    /// Gets or sets whether to request the software adapter. Enable it to render on machines with
    /// no discrete GPU.
    /// </summary>
    public bool UseSoftwareAdapter { get; set; }

    /// <summary>Gets or sets whether to enable backend validation layers.</summary>
    public bool EnableValidation { get; set; }

    /// <summary>
    /// Gets or sets part of the name of the adapter to render on. Null or empty chooses
    /// automatically. No match is a failure rather than a quiet render on another adapter.
    /// </summary>
    public string? AdapterName { get; set; }

    /// <summary>Gets or sets the render deadline in milliseconds. Defaults to 30000.</summary>
    public int TimeoutMilliseconds { get; set; } = 30_000;
}

/// <summary>A rendered projection frame. Dispose it to release the native bytes.</summary>
public sealed class ProjectionFrame : IDisposable
{
    private readonly FrameBuffer _data;

    internal ProjectionFrame(
        int width, int height, long rowPitch, string adapterName, FrameBuffer data)
    {
        Width = width;
        Height = height;
        RowPitch = rowPitch;
        AdapterName = adapterName;
        _data = data;
    }

    /// <summary>Gets the frame width in pixels.</summary>
    public int Width { get; }

    /// <summary>Gets the frame height in pixels.</summary>
    public int Height { get; }

    /// <summary>Gets the number of bytes per row.</summary>
    public long RowPitch { get; }

    /// <summary>
    /// Gets the name of the adapter that drew this frame. When no adapter was named on the
    /// request, this is the only place the choice is reported.
    /// </summary>
    public string AdapterName { get; }

    /// <summary>Copies the packed RGBA8 frame bytes into a managed array.</summary>
    /// <returns>A managed copy of the frame bytes.</returns>
    public byte[] ToArray() => _data.ToArray();

    /// <summary>Gets the number of bytes in the frame.</summary>
    public int Size => _data.Size;

    /// <summary>Releases the native frame bytes.</summary>
    public void Dispose() => _data.Dispose();
}

/// <summary>Handle-free projection entry point.</summary>
public static class Projection
{
    /// <summary>Renders one projection.</summary>
    /// <param name="request">Output size, backend policy, and layers.</param>
    /// <returns>The rendered frame. Dispose it when finished.</returns>
    /// <exception cref="ArgumentNullException"><paramref name="request"/> is null.</exception>
    /// <exception cref="ArgumentException"><paramref name="request"/> has no layers.</exception>
    /// <exception cref="WseException">The native render failed.</exception>
    public static ProjectionFrame Render(ProjectionRequest request)
    {
        ArgumentNullException.ThrowIfNull(request);
        if (request.Layers.Count == 0)
        {
            throw new ArgumentException("A projection needs at least one layer.", nameof(request));
        }

        // Pin every caller array for the duration of the native call. The C ABI reads the pointers
        // during the call only and never retains them.
        var pins = new List<GCHandle>();
        try
        {
            var layers = new NativeProjectionLayer[request.Layers.Count];
            for (int index = 0; index < request.Layers.Count; ++index)
            {
                layers[index] = BuildLayer(request.Layers[index], pins);
            }

            GCHandle layerPin = Pin(layers, pins);

            // A named adapter travels as UTF-8; naming none leaves the pointer null, which the C
            // ABI reads as "choose for me".
            IntPtr adapterName = IntPtr.Zero;
            if (!string.IsNullOrEmpty(request.AdapterName))
            {
                byte[] bytes = new byte[
                    System.Text.Encoding.UTF8.GetByteCount(request.AdapterName) + 1];
                System.Text.Encoding.UTF8.GetBytes(request.AdapterName, 0,
                    request.AdapterName.Length, bytes, 0);
                GCHandle namePin = GCHandle.Alloc(bytes, GCHandleType.Pinned);
                pins.Add(namePin);
                adapterName = namePin.AddrOfPinnedObject();
            }

            var native = new NativeProjectionRequest
            {
                OutputWidth = (uint)request.OutputWidth,
                OutputHeight = (uint)request.OutputHeight,
                ClearColor = new NativeRendererColor
                {
                    Red = request.ClearColor.Red,
                    Green = request.ClearColor.Green,
                    Blue = request.ClearColor.Blue,
                    Alpha = request.ClearColor.Alpha,
                },
                SupersampleScale = (uint)request.SupersampleScale,
                Backend = (int)request.Backend,
                UseSoftwareAdapter = request.UseSoftwareAdapter ? 1 : 0,
                EnableValidation = request.EnableValidation ? 1 : 0,
                AdapterName = adapterName,
                TimeoutMilliseconds = (uint)request.TimeoutMilliseconds,
                Layers = layerPin.AddrOfPinnedObject(),
                LayerCount = (nuint)layers.Length,
            };

            using var output = new NativeOutput(NativeMethods.wse_capi_frame_buffer_destroy);
            NativeMethods.ThrowIfFailed(
                NativeMethods.wse_capi_render_projection(
                    out NativeProjectionFrame frame, out output.Value, ref native));
            var data = new FrameBuffer(output);
            try
            {
                NativeOutput.Checkpoint("projection-result");
                return new ProjectionFrame((int)frame.Width, (int)frame.Height, (long)frame.RowPitch,
                    ReadAdapterName(frame.AdapterName), data);
            }
            catch
            {
                data.Dispose();
                throw;
            }
        }
        finally
        {
            foreach (GCHandle pin in pins)
            {
                pin.Free();
            }
        }
    }

    // The name comes back as a fixed-size array, so it ends at the first terminator rather than
    // at the end of its room.
    private static string ReadAdapterName(byte[]? bytes)
    {
        if (bytes is null) return string.Empty;
        int length = Array.IndexOf(bytes, (byte)0);
        if (length < 0) length = bytes.Length;
        return System.Text.Encoding.UTF8.GetString(bytes, 0, length);
    }

    private static NativeProjectionLayer BuildLayer(ProjectionLayer layer, List<GCHandle> pins)
    {
        var vertices = new NativeProjectionVertex[layer.Vertices.Count];
        for (int index = 0; index < vertices.Length; ++index)
        {
            ProjectionVertex vertex = layer.Vertices[index];
            vertices[index] = new NativeProjectionVertex
            {
                PositionX = vertex.X,
                PositionY = vertex.Y,
                TextureU = vertex.U,
                TextureV = vertex.V,
            };
        }

        var indices = new uint[layer.Indices.Count];
        for (int index = 0; index < indices.Length; ++index)
        {
            indices[index] = (uint)layer.Indices[index];
        }

        GCHandle rgbaPin = Pin(layer.Rgba, pins);
        GCHandle vertexPin = Pin(vertices, pins);
        GCHandle indexPin = Pin(indices, pins);
        IntPtr alphaPointer = IntPtr.Zero;
        nuint alphaSize = 0;
        if (layer.Alpha is { Length: > 0 })
        {
            alphaPointer = Pin(layer.Alpha, pins).AddrOfPinnedObject();
            alphaSize = (nuint)layer.Alpha.Length;
        }

        return new NativeProjectionLayer
        {
            Width = (uint)layer.Width,
            Height = (uint)layer.Height,
            Rgba = rgbaPin.AddrOfPinnedObject(),
            RgbaSize = (nuint)layer.Rgba.Length,
            Alpha = alphaPointer,
            AlphaSize = alphaSize,
            Vertices = vertexPin.AddrOfPinnedObject(),
            VertexCount = (nuint)vertices.Length,
            Indices = indexPin.AddrOfPinnedObject(),
            IndexCount = (nuint)indices.Length,
            SamplingFilter = (int)layer.SamplingFilter,
            Opacity = layer.Opacity,
            EdgeBlend = new NativeEdgeBlend
            {
                Left = layer.EdgeBlend.Left,
                Right = layer.EdgeBlend.Right,
                Top = layer.EdgeBlend.Top,
                Bottom = layer.EdgeBlend.Bottom,
                Curve = (int)layer.EdgeBlend.Curve,
            },
        };
    }

    private static GCHandle Pin(Array value, List<GCHandle> pins)
    {
        GCHandle pin = GCHandle.Alloc(value, GCHandleType.Pinned);
        try
        {
            NativeOutput.Checkpoint("pin-register");
            pins.Add(pin);
            return pin;
        }
        catch
        {
            pin.Free();
            throw;
        }
    }
}
