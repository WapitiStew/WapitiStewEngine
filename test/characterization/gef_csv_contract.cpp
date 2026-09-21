// @file engine/wse/test/characterization/gef_csv_contract.cpp
// @brief GEF CSVの読込、DataMap変換、書出し契約を固定する。
// @details CSVControllerは製品の設定表を外部Fileから読み込む入口である. ここが壊れると, 利用者が
//          用意したCSVの解釈が黙って変わり, 誤ったParameterでCameraやProjectorが動く. 本Testは
//          4つを固定する. 生Cellの行数・列数・文字列がそのまま保持されること, toDataMAP2D()が
//          先頭行をLabelとして扱い1列目をKeyに, 2列目以降SETTING_HEADER未満をLabel名の単一値に,
//          SETTING_HEADER以降を可変長のSETTING_PARAM群にまとめること, write()/read()でCellが
//          保存されること, そしてFile不在と空DataがGefResult契約の安定Codeで報告されることである.

#include <gef/stew.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

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

	//! read()が期待する安定Codeで失敗した場合だけtrueを返す.
	//! 成功して戻った場合はfalseになるため, 黙って成功する実装はここで検出できる.
	bool hasFailureCode(const std::string& path_in, const wse::gef::eGefErrorCode expectedCode_in)
	{
		wse::gef::CSVController csv;
		const wse::gef::GefStatus status = csv.read(path_in);
		return !status.succeeded() && status.error().code() == expectedCode_in;
	}
}

int main(const int argc_in, char** const argv_in)
{
	if (argc_in != 3)
	{
		std::cerr << "FAILED: input and output paths are required\n";
		return 1;
	}

	// 入出力Pathは共にBuild systemから受け取る. 先の実行が残したFileはFixtureの内容と
	// File不在Caseの両方を壊すため, 何かを書く前に必ず消す.
	const std::string inputPath(argv_in[1]);
	const std::string outputPath(argv_in[2]);
	std::remove(inputPath.c_str());
	std::remove(outputPath.c_str());

	// 失敗の報告経路を先に固定する. Data無しの変換とFile不在の読込は, 空のDataMapや
	// 空のCellでは表現できない. 呼び出し側が分岐できるよう別々の安定Codeで届く必要がある.
	wse::gef::CSVController empty;
	const wse::gef::GefResult< wse::gef::datamap_2d > emptyResult = empty.toDataMAP2D();
	expect(
		!emptyResult.succeeded() &&
			emptyResult.error().code() == wse::gef::eGefErrorCode::NoData,
		"An empty CSV cannot be converted to a data map");
	expect(
		hasFailureCode(inputPath, wse::gef::eGefErrorCode::FileOpenFailed),
		"A missing CSV reports FileOpenFailed");

	// 製品の設定表を最小形で再現したFixture. 先頭行がLabel, 1列目がKey, SETTING_HEADER( 4 )
	// 未満の列が固定Label, それ以降が可変長Paramである. camera_bは列数を意図的に1つ減らし,
	// 行ごとにParam数が異なる表を作るために置いてある. この行自体はAssertしていない.
	// FixtureはBinary modeで書き, Platformが改行をCRLFへ変換しないようにしている.
	{
		std::ofstream fixture(inputPath, std::ios::binary);
		fixture << "Key,Category,Num,Remark,Param0,Param1\n";
		fixture << "camera_a,enum,2,primary,10,20\n";
		fixture << "camera_b,int32,1,backup,30\n";
	}

	// read()はCellを解釈せず, 行と列をそのまま保持する. Label行を読み飛ばしたり,
	// 前後の空白を落としたり, 列を詰めたりしないことをここで固定する.
	wse::gef::CSVController csv;
	expect(csv.read(inputPath).succeeded(), "Reading the fixture CSV succeeds");
	expect(csv.contents().size() == 3, "CSV rows are retained");
	expect(csv.contents()[1].size() == 6, "CSV columns are retained");
	expect(csv.contents()[1][0] == "camera_a", "CSV cell text is retained");

	// toDataMAP2D()は表をKey引きの設定へ組み替える. 固定Labelは先頭行の文字列をそのまま
	// 鍵に使い1要素のVectorになる. SETTING_HEADER以降の列は列名では引けず, 行ごとに数が
	// 違うため, SETTING_PARAMという1つの鍵の下に順序を保ったまま束ねられる.
	// この2つの扱いの違いが, 設定表を読む側の書き方を決める.
	const wse::gef::GefResult< wse::gef::datamap_2d > mapResult = csv.toDataMAP2D();
	expect(mapResult.succeeded(), "The fixture CSV converts to a data map");
	const wse::gef::datamap_2d dataMap =
		mapResult.succeeded() ? mapResult.value() : wse::gef::datamap_2d();
	const auto camera = dataMap.find("camera_a");
	expect(camera != dataMap.end(), "Data map contains the row key");
	if (camera != dataMap.end())
	{
		const auto category = camera->second.find("Category");
		const auto parameters = camera->second.find(wse::gef::SETTING_PARAM);
		expect(
			category != camera->second.end() && category->second == std::vector<std::string>{"enum"},
			"Data map contains the labelled category");
		expect(
			parameters != camera->second.end() &&
			parameters->second == std::vector<std::string>({"10", "20"}),
			"Data map groups variable parameters");
	}

	// 書き出したFileを別のControllerで読み直し, Cellが一致することを見る. 比較はCell単位
	// なのでFileのBytesが同一であることまでは主張していない.
	expect(csv.write(outputPath).succeeded(), "Writing the CSV succeeds");
	wse::gef::CSVController roundTrip;
	expect(roundTrip.read(outputPath).succeeded(), "Reading the written CSV succeeds");
	expect(roundTrip.contents() == csv.contents(), "CSV write/read preserves cells");

	std::remove(inputPath.c_str());
	std::remove(outputPath.c_str());
	return failures == 0 ? 0 : 1;
}
