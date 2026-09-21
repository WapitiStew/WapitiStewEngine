// @file engine/wse/test/characterization/wse_map_invariants_contract.cpp
// @brief Mapの形状不変条件と検査付きアクセスの契約を固定する回帰テスト。
// @details Mapが常に保つべき不変条件は width() * height() == size() の一つである.
//          修正前は width*height のOverflowがサイズ検証を偶然通過して不整合なMapを構築でき、
//          空のMapへのfront()/begin()は未定義動作、範囲外はOUT_OF_MEMORYと誤分類されていた.
//          ここでは検査付きの新API (at / row / elements) の境界挙動、Overflowの明示的拒否、
//          範囲外のOUT_OF_RANGE分類、ゼロサイズの扱いを固定する. MapはImage・Matrixの基底であり、
//          この不変条件が破れると座標計算すべてが範囲外読み書きに変わる.

#include <wse/stew.h>

#include <stdexcept>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

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

	// 呼び出しが指定した標準Exceptionで拒否されることを固定する.
	template <typename Exception, typename Callable>
	void expectRejected(Callable&& callable_in, const char* const message_in)
	{
		bool rejected = false;
		try
		{
			callable_in();
		}
		catch (const Exception&)
		{
			rejected = true;
		}
		expect(rejected, message_in);
	}
}

int main()
{
	// --- 検査付き要素アクセス. at(x, y) は行優先で data[y * width + x] を指す.
	{
		wse::F64_MAP map(3, 2);
		map.at(2, 1) = 5.0;
		expect(map.get(1 * 3 + 2) == 5.0, "at() writes the row-major element");
		expect(map[1][2] == 5.0, "at() and operator[] agree on the layout");

		const wse::F64_MAP& constMap = map;
		expect(constMap.at(2, 1) == 5.0, "const at() reads the same element");

		// 行Pointerはwidth()要素分の連続領域を指す.
		map.row(0)[1] = 7.0;
		expect(constMap.row(0)[1] == 7.0, "row() exposes width() consecutive elements");

		// elements()は値アクセスのみ. 全要素が連続領域として見える.
		map.elements()[0] = 9.0;
		expect(constMap.elements()[0] == 9.0 && map.at(0, 0) == 9.0,
			"elements() aliases the same storage");
	}

	// --- 境界の拒否. x側・y側それぞれの境界ちょうどでOUT_OF_RANGEになる.
	{
		wse::F64_MAP map(3, 2);
		expectRejected<std::out_of_range>([&map] { (void)map.at(3, 0); },
			"at() rejects x == width()");
		expectRejected<std::out_of_range>([&map] { (void)map.at(0, 2); },
			"at() rejects y == height()");
		expectRejected<std::out_of_range>([&map] { (void)map.row(2); }, "row() rejects y == height()");
	}

	// --- 範囲外の分類. 修正前のget/setは範囲外をOUT_OF_MEMORY (メモリ不足) と誤報告していた.
	{
		wse::F64_MAP map(2, 2);
		expectRejected<std::out_of_range>([&map] { (void)map.get(4); }, "get() classifies out-of-range as OUT_OF_RANGE");
		expectRejected<std::out_of_range>([&map] { map.set(4, 1.0); },
			"set() classifies out-of-range as OUT_OF_RANGE");
		// 符号付き負値はsize_tへのキャストで巨大な添字になる. UBではなく検査で止まることを固定する.
		expectRejected<std::out_of_range>([&map] { (void)map.get(-1); }, "get() rejects a negative index after conversion");
	}

	// --- 空のMap. 既定構築はサイズ0で、要素アクセスは未定義動作ではなく明示的拒否になる.
	{
		wse::F64_MAP empty;
		expect(empty.size() == 0 && empty.width() == 0 && empty.height() == 0, "Default Map is empty");
		expectRejected<std::out_of_range>([&empty] { (void)empty.front(); }, "front() rejects an empty Map");
		expectRejected<std::out_of_range>([&empty] { (void)empty.at(0, 0); },
			"at() rejects an empty Map");
		expectRejected<std::out_of_range>([&empty] { (void)empty.row(0); }, "row() rejects an empty Map");
	}

	// --- ゼロサイズの片側. 幅0×高さ5は要素0個の正当なMapだが、行Pointerは取得できない.
	{
		wse::F64_MAP zeroWidth(0, 5);
		expect(zeroWidth.size() == 0 && zeroWidth.height() == 5, "Zero-width Map is consistent");
		expectRejected<std::out_of_range>([&zeroWidth] { (void)zeroWidth.row(0); }, "row() rejects a zero-width Map");
	}

	// --- Overflowの拒否. 修正前は width * height が2^64を法として巻き戻り、
	// 小さな確保に大きなwidth/heightが同居する不整合なMapを構築できた.
	{
		const size_t huge = (std::numeric_limits<size_t>::max)() / 2 + 1;
		expectRejected<std::invalid_argument>([huge] { wse::U8_MAP map(huge, 2); (void)map; },
			"Constructor rejects width x height overflow");
		expectRejected<std::invalid_argument>([huge] { wse::U8_MAP map(huge, 2, static_cast<wse::U8>(0)); (void)map; }, "Fill constructor rejects overflow");
		expectRejected<std::invalid_argument>([huge] { wse::U8_MAP map; map.init(huge, 2); }, "init() rejects overflow");
		// 巻き戻った積がちょうどdata.size()と一致する毒入りの組合せ. 2^32 * 2^32 = 0 (mod 2^64)
		// なので空のdataと「一致」し、修正前は検証を通過していた.
		const size_t wrap = static_cast<size_t>(1) << 32;
		expectRejected<std::invalid_argument>([wrap] { wse::U8_MAP map(wrap, wrap, std::vector<wse::U8>{}); (void)map; }, "Data constructor rejects a wrapped product");
	}

	// --- データ構築の不一致は引数不正としてstd::invalid_argumentで拒否する.
	expectRejected<std::invalid_argument>([] { wse::U8_MAP map(2, 2, std::vector<wse::U8>{ 1, 2, 3 }); (void)map; }, "Data constructor rejects a size mismatch");

	// --- 不変条件. 正常な構築・初期化の後は常に width * height == size().
	{
		wse::F64_MAP map(4, 3, 1.5);
		expect(map.width() * map.height() == map.size(), "Shape invariant after fill construction");
		map.init(7, 2);
		expect(map.width() * map.height() == map.size() && map.size() == 14,
			"Shape invariant after init()");
		expect(map.front() == 0.0, "init() value-initializes elements");
	}

	return failures == 0 ? 0 : 1;
}
