// @file engine/wse/test/characterization/core_characterization.cpp
// @brief Phase 1で維持するCore value typeの現行挙動を記録する。
// @details ここに書いた値は仕様書が決めたものではなく、実装がそう振る舞ってきたという記録である.
//          Point2の距離と文字列書式、Range1の境界と長さの定義、Sizeの面積とAspectの型、
//          Pixelのchannel順、bit深度を落とすcastの丸め方 — これらはImage処理、Camera、
//          Rendererを含むCore利用側すべてが前提にしている. 壊れても例外は出ず、
//          座標が1画素ずれる、Logの桁が揃わない、明度が半分になるといった形で静かに現れるため、
//          その静かな変化を検出することがこのFileの役目である.

#include <wse/stew.h>

#include <cmath>
#include <stdexcept>
#include <cstdint>
#include <iostream>
#include <string>

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
}

int main()
{
	// Point2. 既定Constructorが不定値ではなく原点を作ることを固定する.
	const wse::S32_XY origin;
	expect(origin.x == 0 && origin.y == 0, "Point2 default value");

	// 内積・外積と3種の距離. (3,4)はPythagoras数なので、S32のまま計算しても
	// Euclid距離が丸めの影響を受けずちょうど5になり、期待値を整数で書ける.
	// 外積は2次元なのでScalar (x1*y2 - y1*x2 = 15 - 8) を返す.
	const wse::S32_XY point(3, 4);
	expect(point.dot(wse::S32_XY(2, 5)) == 26, "Point2 dot product");
	expect(point.cross(wse::S32_XY(2, 5)) == 7, "Point2 cross product");
	expect(point.manhattan_distance() == 7, "Point2 Manhattan distance");
	expect(point.euclidean_distance() == 5, "Point2 Euclidean distance");
	expect(point.chebyshev_distance() == 4, "Point2 Chebyshev distance");
	// 空白が2つ続くのはTypoではない. 正数にも符号1桁分のSpaceを置く書式なので、
	// 負数と並べたときにLogの桁が揃う. 詰めて出力するよう変えると既存Logの整列が崩れる.
	expect(point.str() == "[  3,  4 ]", "Point2 string representation");

	// Range1. 境界は両端とも内側 (閉区間) だが、length()はmaximum - minimumである.
	// つまり[3,9]は7個の整数を含みながら長さは6を返す. 長さを要素数と読み替えて
	// Loop上限やBuffer長に使うと1つ足りなくなるため、この差をここで固定する.
	const wse::S32_RANGE range(3, 9);
	expect(range.isInner(3) && range.isInner(9), "Range includes both bounds");
	expect(range.isOver(10), "Range rejects an outer value");
	expect(range.length() == 6, "Range length");
	// 逆転した境界はConstructorが例外で拒否する. 黙って入れ替えて正規化はしない.
	// 拾うのはstd::invalid_argumentだけなので、他の型で投げるようになれば例外が抜けてTestが落ちる.
	// 例外が出なければFlagはfalseのままとなり、こちらもExpectationの失敗として現れる.
	bool rejectedReversedRange = false;
	try
	{
		const wse::S32_RANGE invalidRange(9, 3);
		(void)invalidRange;
	}
	catch (const std::invalid_argument& exception)
	{
		rejectedReversedRange = true;
		// what()は理由を運ぶことを固定する. 空文字へ戻るとConsumerのLogから正体が消える.
		expect(std::string(exception.what()).length() > 0, "what() carries the reason");
	}
	expect(rejectedReversedRange, "Range rejects reversed bounds");

	// Size. 1920x1080はint64_sizeでも面積が浮動小数点で返ることを示すための実寸である.
	// aspect()は16:9が2進数で正確に表せないため、等値ではなく1e-6の許容差で比べる.
	// この許容差はfloat64の丸め誤差より十分大きく、比率の取り違えより十分小さい.
	const wse::int64_size frameSize(1920, 1080);
	expect(frameSize.width() == 1920 && frameSize.height() == 1080, "Size dimensions");
	expect(frameSize.area() == 2073600.0, "Size area");
	expect(std::abs(frameSize.aspect() - (16.0 / 9.0)) < 0.000001, "Size aspect ratio");
	expect(!frameSize.empty(), "Non-empty size");
	expect(wse::int64_size().empty(), "Default size is empty");

	// Pixel. pixel_size()はChannel数、pixel_byte()は1画素のByte数であり、
	// 8bit 4Channelではたまたま両方4になる. 別々の概念であることに注意する.
	// 添字0が最初に書いたChannelを指すことも併せて固定する.
	// ここが入れ替わるとRGBAとBGRAが静かに反転する.
	const wse::acolor8_t pixel{{1, 2, 3, 4}};
	expect(pixel.pixel_size() == 4, "Pixel channel count");
	expect(pixel.pixel_byte() == 4, "Pixel byte count");
	expect(pixel[0] == 1 && pixel[3] == 4, "Pixel channel order");

	// bit深度を落とすCast. 64bit -> 16bitの縮小は再Scaleではなく上位bitの取り出しで行うため、
	// 飽和値UINT64_MAXは飽和値UINT16_MAXへ移る. 下位を丸めていたら結果は0付近になる.
	// 出力の第4Channel (Alpha) には元のRGBの平均が入る. すなわちAlphamapが運ぶのは
	// 不透明度ではなく明度であり、これを不透明度と読み替えると合成結果が変わる.
	// 1x1画像なのはこの数値だけを見るためで、近傍の影響は意図的に持ち込んでいない.
	wse::img3c64_t wideColor(1, 1);
	wideColor[0][0][0] = UINT64_MAX;
	wideColor[0][0][1] = UINT64_MAX;
	wideColor[0][0][2] = UINT64_MAX;
	const wse::img4c16_t reducedAlphaMap = wse::castData::to4chAlphamap<
		wse::ePixFormat::CH4D16, wse::ePixFormat::CH3D64>(wideColor);
	expect(reducedAlphaMap[0][0][0] == UINT16_MAX, "64-bit color reduction preserves the high bits");
	expect(reducedAlphaMap[0][0][3] == UINT16_MAX, "Alpha-map reduction fills every channel");

	return failures == 0 ? 0 : 1;
}
