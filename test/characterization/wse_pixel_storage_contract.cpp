// @file engine/wse/test/characterization/wse_pixel_storage_contract.cpp
// @brief 全Pixel FormatのStorage型・Channel数・Bit深度の契約をCompile時に固定する。
// @details CH?D32のStorage型は長らくDoxygenがuint32_tと約束しながら実装はuint64_tであり、
//          32bit画像が1 Channelあたり8 byteを消費していた. Owner決定 (Issue #6) によりuint32_tへ
//          修正したため、このFileが全FormatについてStorage size・DATA_NUM・BIT_DEPTHを
//          static_assertで固定し、同じ乖離が再発したらCompileが失敗するようにする.
//          Byte layoutはBinding・Renderer・Cameraが共有する前提であり、静かに変わってはならない.
//          実行時の検査は縮小Castが上位Bitを保存すること (既存契約と同じ規則) の確認だけである.

#include <wse/stew.h>

#include <cstdint>
#include <iostream>

namespace
{
	int failures = 0;

	// 失敗しても中断せず数え上げる. 1回の実行で壊れた項目をすべて報告するためである.
	void expect(const bool condition_in, const char* const message_in)
	{
		if (!condition_in)
		{
			std::cerr << "FAILED: " << message_in << '\n';
			++failures;
		}
	}

	// 1 Formatぶんの契約をCompile時に固定する. 実行されることはなく、実体化が検査である.
	template <wse::ePixFormat Format, typename Storage, std::uint8_t Channels, std::uint8_t Depth>
	struct PixelContract
	{
		static_assert(std::is_same<wse::PixelType<Format>, Storage>::value,
			"storage type contract");
		static_assert(sizeof(wse::PixelType<Format>) == sizeof(Storage),
			"storage size contract");
		static_assert(wse::PixelSize<Format> == Channels, "channel count contract");
		static_assert(wse::PixelBitDepth<Format> == Depth, "bit depth contract");
		// Pixel全体はChannelの配列そのものであり、詰め物を挟まない.
		static_assert(sizeof(wse::PixelAlias<Format>) == sizeof(Storage) * Channels,
			"pixel stride contract");
	};

	using wse::ePixFormat;

	// D8族: 1 byte storage.
	template struct PixelContract<ePixFormat::CH1D8,   std::uint8_t,  1,  8>;
	template struct PixelContract<ePixFormat::CH2D8,   std::uint8_t,  2,  8>;
	template struct PixelContract<ePixFormat::CH3D8,   std::uint8_t,  3,  8>;
	template struct PixelContract<ePixFormat::CH4D8,   std::uint8_t,  4,  8>;
	template struct PixelContract<ePixFormat::BGR3D8,  std::uint8_t,  3,  8>;
	template struct PixelContract<ePixFormat::BGRA4D8, std::uint8_t,  4,  8>;

	// D10/D12/D14/D16族: 2 byte storage. Bit深度だけが異なる.
	template struct PixelContract<ePixFormat::CH1D10,  std::uint16_t, 1, 10>;
	template struct PixelContract<ePixFormat::CH2D10,  std::uint16_t, 2, 10>;
	template struct PixelContract<ePixFormat::CH3D10,  std::uint16_t, 3, 10>;
	template struct PixelContract<ePixFormat::CH4D10,  std::uint16_t, 4, 10>;
	template struct PixelContract<ePixFormat::CH1D12,  std::uint16_t, 1, 12>;
	template struct PixelContract<ePixFormat::CH2D12,  std::uint16_t, 2, 12>;
	template struct PixelContract<ePixFormat::CH3D12,  std::uint16_t, 3, 12>;
	template struct PixelContract<ePixFormat::CH4D12,  std::uint16_t, 4, 12>;
	template struct PixelContract<ePixFormat::CH1D14,  std::uint16_t, 1, 14>;
	template struct PixelContract<ePixFormat::CH2D14,  std::uint16_t, 2, 14>;
	template struct PixelContract<ePixFormat::CH3D14,  std::uint16_t, 3, 14>;
	template struct PixelContract<ePixFormat::CH4D14,  std::uint16_t, 4, 14>;
	template struct PixelContract<ePixFormat::CH1D16,  std::uint16_t, 1, 16>;
	template struct PixelContract<ePixFormat::CH2D16,  std::uint16_t, 2, 16>;
	template struct PixelContract<ePixFormat::CH3D16,  std::uint16_t, 3, 16>;
	template struct PixelContract<ePixFormat::CH4D16,  std::uint16_t, 4, 16>;
	template struct PixelContract<ePixFormat::BGR3D16, std::uint16_t, 3, 16>;
	template struct PixelContract<ePixFormat::BGRA4D16,std::uint16_t, 4, 16>;

	// D32族: 4 byte storage. かつて実装がuint64_tでDoxygenと乖離していた当事者である.
	template struct PixelContract<ePixFormat::CH1D32,  std::uint32_t, 1, 32>;
	template struct PixelContract<ePixFormat::CH2D32,  std::uint32_t, 2, 32>;
	template struct PixelContract<ePixFormat::CH3D32,  std::uint32_t, 3, 32>;
	template struct PixelContract<ePixFormat::CH4D32,  std::uint32_t, 4, 32>;

	// D64族: 8 byte storage.
	template struct PixelContract<ePixFormat::CH1D64,  std::uint64_t, 1, 64>;
	template struct PixelContract<ePixFormat::CH2D64,  std::uint64_t, 2, 64>;
	template struct PixelContract<ePixFormat::CH3D64,  std::uint64_t, 3, 64>;
	template struct PixelContract<ePixFormat::CH4D64,  std::uint64_t, 4, 64>;
}

int main()
{
	// 32bit画像の実行時像. 1画素1 Channelが4 byteで、UINT32_MAXまでの値を保持できる.
	wse::img1c32_t image32(1, 1);
	image32[0][0][0] = UINT32_MAX;
	expect(image32[0][0][0] == UINT32_MAX, "32-bit channel holds a full 32-bit value");
	expect(image32[0][0].pixel_byte() == 4, "32-bit pixel byte count");
	expect(image32[0][0].pixel_size() == 1, "32-bit pixel channel count");

	// 縮小Castの規則は据え置き: 右Shiftで上位Bitを残す. 64bitの飽和値は32bitの飽和値へ移る.
	{
		wse::img1c64_t wide(1, 1);
		wide[0][0][0] = UINT64_MAX;
		const wse::img1c32_t reduced =
			wse::castData::image<wse::ePixFormat::CH1D32, wse::ePixFormat::CH1D64>(wide);
		expect(reduced[0][0][0] == UINT32_MAX, "64-bit to 32-bit reduction preserves the high bits");
	}

	// 32bitからさらに下げても同じ規則が続く. 飽和値は飽和値のまま降りていく.
	{
		const wse::img1c08_t reduced =
			wse::castData::image<wse::ePixFormat::CH1D08, wse::ePixFormat::CH1D32>(image32);
		expect(reduced[0][0][0] == UINT8_MAX, "32-bit to 8-bit reduction preserves the high bits");
	}

	return failures == 0 ? 0 : 1;
}
