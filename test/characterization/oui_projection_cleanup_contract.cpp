// Copyright (C) 2026 WapitiStew. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#include "../../core/oui/renderer/ProjectionExecution.h"
#include <array>
#include <cstdlib>
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { std::cerr << "line " << __LINE__ << ": " #condition "\n"; std::abort(); } } while (false)
using namespace wse::oui;
namespace
{
enum Stage { Texture, Mesh, First, Second, FreeMesh, FreeTexture, Count };
struct Backend
{
    std::array<int, Count> faults{}; // 0 success, 1 structured error, 2 exception, 3 bad_alloc
    std::array<int, Count> calls{};
    std::array<Stage, Count> trace{};
    unsigned trace_size = 0;
    bool texture_live = false, mesh_live = false;
    sProjectionPassDescription expected;

    bool invoke(Stage stage_in)
    {
        CHECK(trace_size < trace.size());
        trace[trace_size++] = stage_in;
        ++calls[stage_in];
        if (faults[stage_in] == 2) throw stage_in;
        if (faults[stage_in] == 3) throw std::bad_alloc();
        return faults[stage_in] == 0;
    }
    RendererError error(Stage stage_in)
    {
        return {eRendererErrorCategory::Backend, eRendererErrorCode::BackendFailure, "fault", stage_in + 1};
    }
    RendererResult<sTextureHandle> createTexture(const sTextureDescription& description_in)
    {
        CHECK(description_in.extent.width == 8U && description_in.extent.height == 6U);
        CHECK(description_in.format == eRendererPixelFormat::Rgba8Unorm);
        CHECK(!texture_live);
        if (!invoke(Texture)) return RendererResult<sTextureHandle>::failure(error(Texture));
        texture_live = true;
        return RendererResult<sTextureHandle>::success({10U, 1U});
    }
    RendererResult<sMeshHandle> createMesh(const sMeshDescription& description_in)
    {
        CHECK(texture_live && !mesh_live);
        CHECK(description_in.vertices.size() == 4U && description_in.indices.size() == 6U);
        if (!invoke(Mesh)) return RendererResult<sMeshHandle>::failure(error(Mesh));
        mesh_live = true;
        return RendererResult<sMeshHandle>::success({20U, 1U});
    }
    RendererResult<sFenceHandle> executeRenderPass(const sRenderPassDescription& pass_in)
    {
        const Stage stage = calls[First] == 0 ? First : Second;
        if (expected.supersample_scale == 2U)
        {
            CHECK(texture_live && mesh_live);
            CHECK(pass_in.load_operation == eAttachmentLoadOperation::Clear);
            CHECK(pass_in.store_operation == eAttachmentStoreOperation::Store);
            if (stage == First)
            {
                CHECK(pass_in.color_attachment.value == 10U);
                CHECK(pass_in.render_area.extent.width == 8U && pass_in.render_area.extent.height == 6U);
                CHECK(pass_in.final_state == eTextureState::ShaderResource);
                CHECK(pass_in.draw_commands.size() == expected.layers.size());
                for (std::size_t index = 0; index < expected.layers.size(); ++index)
                {
                    CHECK(pass_in.draw_commands[index].mesh.value == expected.layers[index].mesh.value);
                    CHECK(pass_in.draw_commands[index].source_texture.value == expected.layers[index].source_texture.value);
                    CHECK(pass_in.draw_commands[index].blend_mode == eColorBlendMode::SourceAlpha);
                }
            }
            else
            {
                CHECK(pass_in.color_attachment.value == expected.color_attachment.value);
                CHECK(pass_in.render_area.extent.width == 4U && pass_in.render_area.extent.height == 3U);
                CHECK(pass_in.draw_commands.size() == 1U);
                const auto& draw = pass_in.draw_commands.front();
                CHECK(draw.mesh.value == 20U && draw.source_texture.value == 10U);
                CHECK(draw.blend_mode == eColorBlendMode::Replace);
                CHECK(draw.sampling_filter == eTextureSamplingFilter::Linear);
                CHECK(!draw.alpha_texture.valid());
            }
        }
        if (!invoke(stage)) return RendererResult<sFenceHandle>::failure(error(stage));
        return RendererResult<sFenceHandle>::success({static_cast<std::uint64_t>(100 + stage), 1U});
    }
    RendererStatus destroyMesh(sMeshHandle handle_in)
    {
        CHECK(mesh_live && handle_in.value == 20U);
        if (!invoke(FreeMesh)) return RendererStatus::failure(error(FreeMesh));
        mesh_live = false;
        return RendererStatus::success();
    }
    RendererStatus destroyTexture(sTextureHandle handle_in)
    {
        CHECK(texture_live && handle_in.value == 10U);
        if (!invoke(FreeTexture)) return RendererStatus::failure(error(FreeTexture));
        texture_live = false;
        return RendererStatus::success();
    }
};

sProjectionPassDescription description()
{
    sProjectionPassDescription pass;
    pass.color_attachment = {1000U, 1U};
    pass.render_area.extent = {4U, 3U};
    pass.supersample_scale = 2U;
    pass.layers.emplace_back(sMeshHandle{1001U, 1U}, sTextureHandle{1002U, 1U});
    pass.layers.emplace_back(sMeshHandle{1003U, 1U}, sTextureHandle{1004U, 1U});
    return pass;
}
}
int main()
{
    const auto pass = description();
    // Every stage can return a status or throw. Combine primary failures with both
    // cleanup failures: cleanup must never replace the original result/exception.
    for (int primary = Texture; primary <= Second; ++primary)
    for (int mode = 1; mode <= 3; ++mode)
    for (int mesh_cleanup = 0; mesh_cleanup <= 2; ++mesh_cleanup)
    for (int texture_cleanup = 0; texture_cleanup <= 2; ++texture_cleanup)
    {
        Backend backend;
        backend.expected = pass;
        backend.faults[primary] = mode;
        backend.faults[FreeMesh] = mesh_cleanup;
        backend.faults[FreeTexture] = texture_cleanup;
        try
        {
            const auto result = detail::executeProjection(&backend, pass);
            CHECK(mode == 1 && !result.succeeded());
            CHECK(result.error().nativeCode() == primary + 1);
        }
        catch (Stage exception) { CHECK(mode == 2 && exception == primary); }
        catch (const std::bad_alloc&) { CHECK(mode == 3); }
        CHECK(backend.calls[FreeTexture] == (primary > Texture ? 1 : 0));
        CHECK(backend.calls[FreeMesh] == (primary > Mesh ? 1 : 0));
        CHECK(!backend.texture_live || texture_cleanup != 0);
        CHECK(!backend.mesh_live || mesh_cleanup != 0);
    }
    // Both successful passes: errors prefer mesh; exceptions prefer the first throw.
    for (int mesh_cleanup = 0; mesh_cleanup <= 2; ++mesh_cleanup)
    for (int texture_cleanup = 0; texture_cleanup <= 2; ++texture_cleanup)
    {
        Backend backend;
        backend.expected = pass;
        backend.faults[FreeMesh] = mesh_cleanup;
        backend.faults[FreeTexture] = texture_cleanup;
        try
        {
            const auto result = detail::executeProjection(&backend, pass);
            CHECK(mesh_cleanup != 2 && texture_cleanup != 2);
            if (mesh_cleanup == 0 && texture_cleanup == 0)
                CHECK(result.succeeded() && result.value().value == 100U + Second);
            else
                CHECK(!result.succeeded() && result.error().nativeCode() ==
                    (mesh_cleanup == 1 ? FreeMesh : FreeTexture) + 1);
        }
        catch (Stage exception)
        {
            CHECK(mesh_cleanup == 2 || texture_cleanup == 2);
            CHECK(exception == (mesh_cleanup == 2 ? FreeMesh : FreeTexture));
        }
        CHECK(backend.trace_size == Count);
        for (int stage = 0; stage < Count; ++stage)
            CHECK(backend.calls[stage] == 1 && backend.trace[stage] == stage);
        CHECK(backend.mesh_live == (mesh_cleanup != 0));
        CHECK(backend.texture_live == (texture_cleanup != 0));
    }
    Backend backend;
    backend.expected = pass;
    backend.expected.supersample_scale = 1U;
    CHECK(detail::executeProjection(&backend, backend.expected).succeeded());
    CHECK(backend.trace_size == 1U && backend.calls[First] == 1);
    backend.expected.layers.clear();
    CHECK(!detail::executeProjection(&backend, backend.expected).succeeded());
    CHECK(backend.trace_size == 1U);
    CHECK(!detail::executeProjection<Backend>(nullptr, pass).succeeded());
    std::cout << "117 fault matrix cases, scale 1, validation and call order passed.\n";
}
