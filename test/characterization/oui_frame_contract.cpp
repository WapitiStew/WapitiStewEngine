// @file engine/wse/test/characterization/oui_frame_contract.cpp
// @brief Projection境界で使うFrame descriptionの非GPU挙動を記録する。
// sFrameDescのByte深さ切り上げとRow／Frame Byte数の算出は、Projectionへ渡すCPU Bufferの
// 確保量そのものである. ここが崩れるとBuffer不足による書き潰しや、Stride不一致による映像の
// 斜めずれが起きるため、GPUを一切使わない算術契約として固定する.

#include <oui/renderer/utility/RenderTypes.h>
#include "../support/ProjectionReadbackCheck.h"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <array>
#include <algorithm>

namespace
{
	int failures = 0;

	void expect(const bool condition_in, const char* const message_in)
	{
		if (!condition_in)
		{
			std::cerr << "FAILED: " << message_in << '\n';
			++failures;
		}
	}
}

int main()
{
	// 既定構築はSize未設定のRGBA8である. Projection側はChannel数とBit深さを省略して
	// 渡してくるため、この既定が変わると呼び出し側の意図しないFormatになる.
	wse::oui::sFrameDesc frame;
	expect(frame.size.empty(), "Default frame size");
	expect(frame.channels == 4, "Default frame channels");
	expect(frame.bit_depth == 8, "Default frame bit depth");

	// 1080p RGBA8の基準値. Row 7680 = 1920 x 4 Channel x 1 Byte、
	// Frame 8294400 = 1080 Row x 7680であり、Padding無しのPacked layoutを固定する.
	frame.size = wse::int64_size(1920, 1080);
	expect(frame.byte_depth() == 1, "8-bit byte depth");
	expect(frame.row_memory_size() == 7680, "RGBA8 row size");
	expect(frame.memory_size() == 8294400, "RGBA8 frame size");

	// 10 bitは1 Byteへ収まらないのでByte深さは2へ切り上がる. Bit詰めではなくChannel当たり
	// 2 Byteで保持する契約であり、Row 11520 = 1920 x 3 x 2、Frame 12441600 = 1080 x 11520となる.
	// 切り上げを切り捨てへ誤ると10 bit Sourceで確保量が半分になる.
	frame.channels = 3;
	frame.bit_depth = 10;
	expect(frame.byte_depth() == 2, "10-bit storage byte depth");
	expect(frame.row_memory_size() == 11520, "RGB10 row storage size");
	expect(frame.memory_size() == 12441600, "RGB10 frame storage size");

	// Copy構築がSize／Channel数／Bit深さの3 Memberを全て運ぶことをoperator==で確かめる.
	// Frame descriptionはBinding境界を値渡しで越えるため、欠落は受け側の確保量誤りになる.
	const wse::oui::sFrameDesc copy(frame);
	expect(copy == frame, "Frame description copy equality");

    // Prove the hardware predicate rejects the old alpha-only false positive,
    // channel swaps and spatial permutations without opening a display.
    for( const bool bgra : { false, true } )
    {
        wse::oui::sRendererFrame pattern;
        pattern.description.extent = { 64U, 48U };
        pattern.description.format = bgra ? wse::oui::eRendererPixelFormat::Bgra8Unorm
            : wse::oui::eRendererPixelFormat::Rgba8Unorm;
        pattern.description.row_pitch = 64U * 4U + 16U;
        pattern.data.resize( pattern.description.row_pitch * 48U, 0xEEU );
        const std::array< std::array< std::uint8_t, 3 >, 4 > colors{{
            {{255,96,0}}, {{0,255,96}}, {{0,96,255}}, {{255,255,255}}
        }};
        for( unsigned y = 0; y < 48U; ++y )
        for( unsigned x = 0; x < 64U; ++x )
        {
            auto* pixel = pattern.data.data() + y * pattern.description.row_pitch + x * 4U;
            const auto& rgb = colors[ ( y >= 24U ? 2U : 0U ) + ( x >= 32U ? 1U : 0U ) ];
            pixel[ bgra ? 2U : 0U ] = rgb[0]; pixel[1] = rgb[1]; pixel[ bgra ? 0U : 2U ] = rgb[2];
            pixel[3] = 7U; // Alpha deliberately differs from the source texture.
        }
        std::ostringstream diagnostic;
        expect( wse_test::matchesProjectionPattern( pattern, diagnostic ), "RGBA/BGRA padded spatial pattern passes independently of alpha" );
        auto bad = pattern;
        for( unsigned y = 0; y < 48U; ++y )
        for( unsigned x = 0; x < 64U; ++x )
        {
            auto* pixel = bad.data.data() + y * bad.description.row_pitch + x * 4U;
            pixel[0] = pixel[1] = pixel[2] = 0; pixel[3] = 255U;
        }
        expect( !wse_test::matchesProjectionPattern( bad, diagnostic ), "Opaque black cannot pass on alpha variation" );
        bad = pattern;
        for( unsigned y = 0; y < 48U; ++y )
        for( unsigned x = 0; x < 64U; ++x )
        {
            auto* pixel = bad.data.data() + y * bad.description.row_pitch + x * 4U;
            std::swap( pixel[0], pixel[2] );
        }
        expect( !wse_test::matchesProjectionPattern( bad, diagnostic ), "Red/blue channel swap is rejected" );
        bad = pattern;
        for( unsigned y = 0; y < 24U; ++y )
            std::swap_ranges( bad.data.begin() + y * bad.description.row_pitch,
                bad.data.begin() + ( y + 1U ) * bad.description.row_pitch,
                bad.data.begin() + ( 47U - y ) * bad.description.row_pitch );
        expect( !wse_test::matchesProjectionPattern( bad, diagnostic ), "Vertically mirrored pattern is rejected" );
        bad = pattern; bad.data.pop_back();
        expect( !wse_test::matchesProjectionPattern( bad, diagnostic ), "Truncated readback is rejected" );
    }

	return failures == 0 ? 0 : 1;
}
