// @file engine/wse/test/benchmark/tmr_benchmark.cpp
// @brief TmrのFrame処理Hot pathを合成Frameで計測する (PERF-002).
// @details 実DeviceのCapture FPS／Frame latency／Controlは実機Phase (HW検証) の対象なので、
//          ここではDeviceなしで測れる部分 - Image→Frame→Imageの往復変換 - を記録する.
//          これはCallback内で毎Frame払うCPU Costであり、Capture pipelineの下限を決める.
//          判定はしない - 出力TSVがCommit間比較の材料である.

#include <tmr/stew.h>

#include "benchmark_support.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace
{
	volatile std::uint64_t sink = 0;
	void require(const bool value_in)
	{
		if (!value_in) { std::fputs("Frame conversion failed\n", stderr); std::exit(1); }
	}
}

int main()
{
	using namespace wse::tmr;

	// 640x480のRGB画像を決定的な階調で用意する.
	wse::img3c08_t source(640, 480);
	for (std::size_t y = 0; y < source.height(); ++y)
	{
		for (std::size_t x = 0; x < source.width(); ++x)
		{
			source[y][x][0] = static_cast<std::uint8_t>(x & 0xFF);
			source[y][x][1] = static_cast<std::uint8_t>(y & 0xFF);
			source[y][x][2] = static_cast<std::uint8_t>((x + y) & 0xFF);
		}
	}
	const std::uint64_t frame_bytes = 640ULL * 480ULL * 3ULL;
	const auto reference = toCameraFrame(source);
	require(reference.succeeded());
	wse::img3c08_t roundtrip;
	require(toImage(&roundtrip, reference.value()).succeeded());
	for (std::size_t y = 0; y < source.height(); ++y)
		for (std::size_t x = 0; x < source.width(); ++x)
			for (std::size_t c = 0; c < 3; ++c)
				require(roundtrip[y][x][c] == source[y][x][c]);

	// --- Image -> Frame. Callbackへ渡すFrameの構築Cost.
	{
		wse_bench::run("tmr.frame.from_image_3c08_640x480", 3, 50, [&] {
			const CameraResult<sCameraFrame> frame = toCameraFrame(source);
			require(frame.succeeded() && frame.value().data.size() == frame_bytes);
			sink += frame.value().data.back();
		}, frame_bytes);
	}

	// --- Frame -> Image. Consumerが毎Frame払う取り出しCost.
	{
		const CameraResult<sCameraFrame> frame = toCameraFrame(source);
		require(frame.succeeded());
		wse::img3c08_t decoded(1, 1);
		wse_bench::run("tmr.frame.to_image_3c08_640x480", 3, 50, [&] {
			require(toImage(&decoded, frame.value()).succeeded());
			sink += decoded[479][639][2];
		}, frame_bytes);
	}

	std::printf("# tmr benchmark complete (capture FPS / control latency are hardware-phase items)\n");
	std::printf("# sink=%llu\n", static_cast<unsigned long long>(sink));
	return 0;
}
