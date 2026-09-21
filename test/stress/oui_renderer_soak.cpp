// @file engine/wse/test/stress/oui_renderer_soak.cpp
// @brief WARP Rendererのresource create/destroyとinitialize/shutdownを大量反復し、
//        Handle枯渇・Leak・Device lost非対応がないことを固定する.
// @details 物理GPUや実Displayは実機Phaseの対象なので、ここはVendor非依存のSoftware adapter (WARP)
//          だけを使う. Texture生成解放を数千回、Renderer全体のinitialize/shutdownを数十回通し、
//          反復でしか出ない解放漏れや再初期化の破綻を炙り出す. TIMEOUTがDeadlock/暴走検出器である.

#include <oui/stew.h>

#include <iostream>
#include <string>

namespace
{
	int failures = 0;

	void expect(const bool condition_in, const std::string& message_in)
	{
		if (!condition_in)
		{
			std::cerr << "FAILED: " << message_in << '\n';
			++failures;
		}
	}

	wse::oui::sRendererConfiguration warpConfiguration()
	{
		wse::oui::sRendererConfiguration configuration;
		configuration.backend = wse::oui::eRendererBackend::Direct3D12;
		configuration.use_software_adapter = true;
		return configuration;
	}

	wse::oui::sTextureDescription textureDescription()
	{
		wse::oui::sTextureDescription description;
		description.extent = { 64U, 64U };
		description.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
		description.usage = wse::oui::eTextureUsage::RenderTarget |
			wse::oui::eTextureUsage::TransferSource;
		description.initial_state = wse::oui::eTextureState::RenderTarget;
		return description;
	}
}

int main()
{
	// WARPが無い環境ではRenderer自体が成立しないので、初期化失敗はSkipではなく失敗にする.
	wse::oui::Renderer renderer;
	const auto initialize_result = renderer.initialize(warpConfiguration());
	if (!initialize_result.succeeded())
	{
		std::cerr << "FAILED: WARP renderer initialize: "
			<< initialize_result.error().message() << '\n';
		return 1;
	}

	// --- Texture create/destroyの大量反復. Handleが再利用され、解放漏れが累積しない.
	{
		const wse::oui::sTextureDescription description = textureDescription();
		bool all_ok = true;
		for (int i = 0; i < 4000 && all_ok; ++i)
		{
			const auto texture = renderer.createTexture(description);
			if (!texture.succeeded())
			{
				expect(false, "createTexture failed at iteration " + std::to_string(i));
				all_ok = false;
				break;
			}
			if (!renderer.destroyTexture(texture.value()).succeeded())
			{
				expect(false, "destroyTexture failed at iteration " + std::to_string(i));
				all_ok = false;
			}
		}
		expect(all_ok, "4000 texture create/destroy cycles complete");
	}

	// --- 多数同時生存 -> 一括解放. Poolが枯渇せず、まとめて解放しても壊れない.
	{
		std::vector<wse::oui::sTextureHandle> handles;
		const wse::oui::sTextureDescription description = textureDescription();
		bool created = true;
		for (int i = 0; i < 256 && created; ++i)
		{
			const auto texture = renderer.createTexture(description);
			if (!texture.succeeded())
			{
				created = false;
				break;
			}
			handles.push_back(texture.value());
		}
		expect(created && handles.size() == 256, "256 textures live simultaneously");
		bool destroyed = true;
		for (const wse::oui::sTextureHandle handle : handles)
		{
			destroyed = destroyed && renderer.destroyTexture(handle).succeeded();
		}
		expect(destroyed, "all 256 textures destroy cleanly");
		// 解放済みHandleの二重解放は拒否される (Handle再利用による巻き添えを防ぐ).
		if (!handles.empty())
		{
			expect(!renderer.destroyTexture(handles.front()).succeeded(),
				"double destroy is rejected");
		}
	}

	renderer.shutdown();

	// --- Renderer全体のinitialize/shutdownの反復. Device生成破棄でLeakやDevice lost非対応が出ない.
	{
		bool all_ok = true;
		for (int cycle = 0; cycle < 40 && all_ok; ++cycle)
		{
			wse::oui::Renderer cycled;
			if (!cycled.initialize(warpConfiguration()).succeeded())
			{
				expect(false, "re-initialize failed at cycle " + std::to_string(cycle));
				all_ok = false;
				break;
			}
			const auto texture = cycled.createTexture(textureDescription());
			all_ok = texture.succeeded() &&
				cycled.destroyTexture(texture.value()).succeeded();
			cycled.shutdown();
		}
		expect(all_ok, "40 renderer initialize/shutdown cycles complete");
	}

	std::cout << "oui renderer soak complete\n";
	return failures == 0 ? 0 : 1;
}
