// @file engine/wse/test/characterization/wse_matrix_correctness_contract.cpp
// @brief Matrix_の数値計算の正しさを固定する回帰・性質テスト。
// @details ここで固定するのは修正済みの既知不具合の再発防止と、行列演算が満たすべき数学的性質である.
//          Covarianceの入力検証 (空・1サンプル・次元不一致・null出力)、スカラー版が常に例外を
//          投げていた要素二重生成の修正、逆行列のピボット行交換 (自己swapで無効化されていた)、
//          特異・準特異行列の明示的な拒否、O(n!)だった行列式の次元耐性 — これらが対象である.
//          行列演算はCamera校正やProjection計算の土台であり、壊れると座標変換全体が静かに歪む.

#include <wse/stew.h>
#include <wse/error/CoreError.h>

#include <cmath>
#include <stdexcept>
#include <cstdint>
#include <iostream>
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

	// 使い方違反がstd::invalid_argumentで拒否されることを固定する.
	// 他の型で投げるようになれば例外が抜けてTestが落ち、投げなければ期待失敗として現れる.
	template <typename Callable>
	void expectInvalidArgument(Callable&& callable_in, const char* const message_in)
	{
		bool rejected = false;
		try
		{
			callable_in();
		}
		catch (const std::invalid_argument&)
		{
			rejected = true;
		}
		expect(rejected, message_in);
	}

	// tryInverse()が特異として失敗Resultを返すことを固定する. Exceptionでは報告しない.
	template <typename MatrixType>
	void expectSingular(const MatrixType& matrix_in, const char* const message_in)
	{
		const auto result = matrix_in.tryInverse();
		expect(!result.succeeded()
			&& result.error().category() == wse::eCoreErrorCategory::Computation
			&& result.error().code() == wse::eCoreErrorCode::SingularMatrix, message_in);
	}

	// 全要素の一致を許容誤差付きで確認する. 逆行列は消去の丸めを含むため厳密比較はできない.
	bool nearlyEqual(const wse::Matrix& actual_in, const wse::Matrix& expected_in, const double tolerance_in)
	{
		if ((actual_in.rows() != expected_in.rows()) || (actual_in.cols() != expected_in.cols()))
		{
			return false;
		}
		for (size_t row = 0; row < actual_in.rows(); ++row)
		{
			for (size_t col = 0; col < actual_in.cols(); ++col)
			{
				if (std::abs(actual_in[row][col] - expected_in[row][col]) > tolerance_in)
				{
					return false;
				}
			}
		}
		return true;
	}

	// 決定的な擬似乱数. 実行ごとに違う行列でTestが明滅しないよう、seed固定のLCGを使う.
	std::uint64_t lcgState = 0x2545F4914F6CDD1DULL;
	double nextRandom()
	{
		lcgState = lcgState * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<double>((lcgState >> 33) % 2000) / 100.0 - 10.0;
	}
}

int main()
{
	// --- Covariance: 入力検証. 修正前は空入力でdata_in[0]に触れてUB、1サンプルはZeroDevidに化けていた.
	expectInvalidArgument([] {
		wse::Matrix mean(1, 1);
		(void)wse::Matrix::Covariance(&mean, std::vector<wse::Matrix>{});
	}, "Covariance rejects an empty sample set");

	expectInvalidArgument([] {
		(void)wse::Matrix::Covariance(static_cast<wse::Matrix*>(nullptr),
			std::vector<wse::Matrix>{ wse::Matrix(2, 1, 1.0), wse::Matrix(2, 1, 2.0) });
	}, "Covariance rejects a null mean output");

	// 標本共分散はN-1で割るため1サンプルでは定義できない. 黙って0除算にせず引数として拒否する.
	expectInvalidArgument([] {
		wse::Matrix mean(1, 1);
		(void)wse::Matrix::Covariance(&mean, std::vector<wse::Matrix>{ wse::Matrix(2, 1, 1.0) });
	}, "Covariance rejects a single sample");

	expectInvalidArgument([] {
		wse::Matrix mean(1, 1);
		(void)wse::Matrix::Covariance(&mean,
			std::vector<wse::Matrix>{ wse::Matrix(2, 1, 1.0), wse::Matrix(3, 1, 2.0) });
	}, "Covariance rejects mismatched sample dimensions");

	expectInvalidArgument([] {
		wse::Matrix mean(1, 1);
		(void)wse::Matrix::Covariance(&mean,
			std::vector<wse::Matrix>{ wse::Matrix(2, 2, 1.0), wse::Matrix(2, 2, 2.0) });
	}, "Covariance rejects non-column samples");

	// --- Covariance: 2サンプルの既知値. [1,2]と[3,6]の平均は[2,4]、
	// 偏差[-1,-2]と[1,2]の外積和をN-1=1で割ると[[2,4],[4,8]]になる.
	{
		wse::Matrix first(2, 1);
		first[0][0] = 1.0; first[1][0] = 2.0;
		wse::Matrix second(2, 1);
		second[0][0] = 3.0; second[1][0] = 6.0;

		wse::Matrix mean(1, 1);
		const wse::Matrix covariance =
			wse::Matrix::Covariance(&mean, std::vector<wse::Matrix>{ first, second });

		expect(mean.rows() == 2 && mean.cols() == 1, "Covariance mean shape");
		expect(mean[0][0] == 2.0 && mean[1][0] == 4.0, "Covariance mean values");

		wse::Matrix expected(2, 2);
		expected[0][0] = 2.0; expected[0][1] = 4.0;
		expected[1][0] = 4.0; expected[1][1] = 8.0;
		expect(nearlyEqual(covariance, expected, 0.000000001), "Covariance known two-sample values");
	}

	// --- Covariance: スカラー版. 修正前はサイズ指定構築とemplace_backの併用で要素が二重になり、
	// 先頭に並んだ空行列が検証に引っ掛かって非空入力でも常に例外を投げていた.
	{
		double mean = 0.0;
		const wse::Matrix covariance =
			wse::Matrix::Covariance(&mean, std::vector<double>{ 1.0, 3.0 });
		expect(mean == 2.0, "Scalar covariance mean");
		expect(covariance.rows() == 1 && covariance.cols() == 1, "Scalar covariance shape");
		expect(std::abs(covariance[0][0] - 2.0) < 0.000000001, "Scalar covariance value");
	}

	expectInvalidArgument([] {
		double mean = 0.0;
		(void)wse::Matrix::Covariance(&mean, std::vector<double>{});
	}, "Scalar covariance rejects an empty sample set");

	// --- inverse: ピボット行交換が必要な行列. [0][0]が0なので行交換なしでは計算できない.
	// 修正前は自己swapで行交換が無効化されており、この形はCanNotCalc_InverseMatrixに化けていた.
	{
		wse::Matrix pivotMatrix(3, 3);
		pivotMatrix[0][0] = 0.0; pivotMatrix[0][1] = 1.0; pivotMatrix[0][2] = 2.0;
		pivotMatrix[1][0] = 1.0; pivotMatrix[1][1] = 0.0; pivotMatrix[1][2] = 3.0;
		pivotMatrix[2][0] = 4.0; pivotMatrix[2][1] = -3.0; pivotMatrix[2][2] = 8.0;

		const wse::Matrix inverse = pivotMatrix.tryInverse().value();
		expect(nearlyEqual(pivotMatrix * inverse, wse::Matrix::Identify(3), 0.000000001),
			"Inverse of a matrix requiring a pivot row swap");
	}

	// --- inverse: 1x1と2x2の直接式.
	{
		wse::Matrix scalarMatrix(1, 1, 4.0);
		expect(std::abs(scalarMatrix.tryInverse().value()[0][0] - 0.25) < 0.000000001, "1x1 inverse");
		expect(scalarMatrix.determinant() == 4.0, "1x1 determinant");

		wse::Matrix swapMatrix(2, 2);
		swapMatrix[0][0] = 0.0; swapMatrix[0][1] = 1.0;
		swapMatrix[1][0] = 1.0; swapMatrix[1][1] = 0.0;
		expect(swapMatrix.determinant() == -1.0, "2x2 permutation determinant");
		expect(nearlyEqual(swapMatrix.tryInverse().value(), swapMatrix, 0.000000001), "2x2 permutation inverse");
	}

	// --- tryInverse: 特異行列の失敗Result. 厳密に特異な行列と、消去後のピボットが丸め誤差に
	// 埋もれる行列の両方. 正常な入力でも起こり得るため、ExceptionではなくCoreResultで報告する.
	{
		wse::Matrix singular2(2, 2);
		singular2[0][0] = 1.0; singular2[0][1] = 2.0;
		singular2[1][0] = 2.0; singular2[1][1] = 4.0;
		expectSingular(singular2, "2x2 singular matrix reports SingularMatrix");

		wse::Matrix singular3(3, 3);
		singular3[0][0] = 1.0; singular3[0][1] = 2.0; singular3[0][2] = 3.0;
		singular3[1][0] = 4.0; singular3[1][1] = 5.0; singular3[1][2] = 6.0;
		singular3[2][0] = 7.0; singular3[2][1] = 8.0; singular3[2][2] = 9.0;
		expectSingular(singular3, "3x3 rank-deficient matrix reports SingularMatrix");

		expectSingular(wse::Matrix::Square(3), "Zero matrix reports SingularMatrix");
	}

	// 非正方は使い方違反であり、失敗Resultではなくstd::invalid_argumentで拒否する.
	expectInvalidArgument([] {
		(void)wse::Matrix(2, 3, 1.0).tryInverse();
	}, "Non-square matrix is rejected as a usage violation");

	// F32では1e-7の行列式が丸め誤差の規模に埋もれる. 準特異を黙って巨大値にしないことを固定する.
	{
		wse::Matrix_f nearSingular(2, 2);
		nearSingular[0][0] = 1.0f; nearSingular[0][1] = 1.0f;
		nearSingular[1][0] = 1.0f; nearSingular[1][1] = 1.0000001f;
		expectSingular(nearSingular, "F32 near-singular matrix reports SingularMatrix");
	}

	// --- determinant: 既知値と転置不変性. [[1,2,3],[4,5,6],[7,8,10]]の行列式は-3.
	{
		wse::Matrix known(3, 3);
		known[0][0] = 1.0; known[0][1] = 2.0; known[0][2] = 3.0;
		known[1][0] = 4.0; known[1][1] = 5.0; known[1][2] = 6.0;
		known[2][0] = 7.0; known[2][1] = 8.0; known[2][2] = 10.0;
		expect(std::abs(known.determinant() - (-3.0)) < 0.000000001, "3x3 determinant known value");
		expect(std::abs(known.transpose().determinant() - (-3.0)) < 0.000000001,
			"Determinant is transpose-invariant");
		expect(nearlyEqual(known * known.tryInverse().value(), wse::Matrix::Identify(3), 0.000000001),
			"3x3 inverse cross-check");

		// 奇置換の符号. 行交換1回分の符号反転がLU化でも保存されることを固定する.
		wse::Matrix permutation = wse::Matrix::Square(3);
		permutation[0][2] = 1.0; permutation[1][1] = 1.0; permutation[2][0] = 1.0;
		expect(std::abs(permutation.determinant() - (-1.0)) < 0.000000001,
			"3x3 permutation determinant sign");
	}

	// --- determinant: 次元耐性. 余因子展開はO(n!)で12次では10^8回の再帰になりTimeoutする.
	// 消去法ならば即座に終わる. この行が5秒のTest timeout内で通ること自体が性能の回帰検知である.
	expect(std::abs(wse::Matrix::Identify(12).determinant() - 1.0) < 0.000000001,
		"12x12 identity determinant completes within the timeout");

	// --- 性質: seed固定の擬似乱数行列で A * inverse(A) ≈ I. 対角優位にして可逆性を保証する.
	{
		const size_t dimension = 5;
		wse::Matrix randomMatrix(dimension, dimension);
		for (size_t row = 0; row < dimension; ++row)
		{
			for (size_t col = 0; col < dimension; ++col)
			{
				randomMatrix[row][col] = nextRandom();
			}
			randomMatrix[row][row] += 100.0;
		}
		expect(nearlyEqual(randomMatrix * randomMatrix.tryInverse().value(),
			wse::Matrix::Identify(dimension), 0.000000001),
			"Random diagonally-dominant matrix inverse property");
	}

	return failures == 0 ? 0 : 1;
}
