// @file engine/wse/test/characterization/tmr_data_contract.cpp
// @brief 実Cameraへ接続せずTmrのParameterとColorMatrix契約を固定する。
// ParameterはCameraの各制御について、対応するControl mode、Range、Step、設定可能値をUIとBindingへ
// 運ぶ値である. これが壊れると、非対応のControl modeをSupportedとして公開したり、
// List型Parameterに対する数値Range照会がExceptionではなく無意味な値で通ってしまう.
// ColorMatrixはrgb2xyz()とxyz2rgb()が互いの逆変換であり続けることを固定する.
// これが壊れると、色変換を往復した画素が元へ戻らなくなる.

#include <tmr/data/ColorMatrix.h>
#include <tmr/data/Parameter.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

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

	bool near(const double actual_in, const double expected_in)
	{
		return std::abs(actual_in - expected_in) < 0.000001;
	}
}

int main()
{
	using Parameter = wse::tmr::Parameter<std::int32_t>;
	Parameter value(
		true,
		false,
		true,
		wse::tmr::eParamType::VALUE,
		wse::tmr::eControlMode::MANUAL,
		10,
		2,
		12,
		std::vector<std::int32_t>{0, 100});
	expect(value.isSupport(), "Parameter reports supported controls");
	expect(value.isSupport(wse::tmr::eControlMode::MANUAL), "Manual mode is supported");
	expect(!value.isSupport(wse::tmr::eControlMode::AUTO), "Auto mode is unsupported");
	expect(value.isSupport(wse::tmr::eControlMode::AUTO_ADJUST), "Auto-adjust mode is supported");
	expect(value.min_value() == 0 && value.max_value() == 100, "Value range endpoints");
	expect(value.init_value() == 10 && value.step() == 2 && value.current_value() == 12, "Value metadata");

	value.setControlMode(wse::tmr::eControlMode::AUTO, true);
	value.setCurrentValue(14);
	expect(value.isSupport(wse::tmr::eControlMode::AUTO), "Control support can be enabled");
	expect(value.current_value() == 14, "Current value can be updated");
	const Parameter copy(value);
	expect(copy.supported_control_bitfield() == value.supported_control_bitfield(), "Parameter copy support bits");
	expect(copy.settable_value_list() == value.settable_value_list(), "Parameter copy value list");

	const Parameter listed(
		true,
		false,
		false,
		wse::tmr::eParamType::LIST,
		wse::tmr::eControlMode::MANUAL,
		1,
		0,
		3,
		std::vector<std::int32_t>{1, 3, 5});
	bool rejectedListMaximum = false;
	try
	{
		(void)listed.max_value();
	}
	catch (const std::invalid_argument&)
	{
		rejectedListMaximum = true;
	}
	expect(rejectedListMaximum, "List parameter rejects numeric maximum access");

	const wse::float64_matrix identity(3, 3, {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0
	});
	const wse::tmr::ColorMatrix color(identity);
	const wse::double_xyz xyz = color.rgb2xyz(0.25, 0.5, 0.75);
	expect(near(xyz.x, 0.25) && near(xyz.y, 0.5) && near(xyz.z, 0.75), "Identity RGB to XYZ");
	const std::array<double, 3> rgb = color.xyz2rgb(xyz);
	expect(near(rgb[0], 0.25) && near(rgb[1], 0.5) && near(rgb[2], 0.75), "Identity XYZ to RGB");

	return failures == 0 ? 0 : 1;
}
