// @file engine/wse/test/characterization/tmr_calibration_contract.cpp
// @brief Device I/Oを必要としないPortable Tmr Calibration値契約を固定する。
// sCameraCalibrationはCameraを開かずに保存、転送、比較できるOwned valueであり、
// valid()がその値を適用してよいかどうかの唯一の門番になる.
// この契約が壊れると、退化したIntrinsics、非有限のDistortion係数、要素数の合わないLens shading mapが
// valid()を通過し、Calibrationを適用するPipelineがDevice I/Oの手前で弾けない破損値を受け取る.

#include <tmr/calibration/CameraCalibration.h>

#include <iostream>
#include <limits>

namespace
{
int failures = 0;

void expect( const bool condition_in, const char* const message_in )
{
    if( !condition_in )
    {
        std::cerr << "FAILED: " << message_in << '\n';
        ++failures;
    }
}
}

int main()
{
    using namespace wse::tmr;

    // 4つのCalibration domainをすべて備えた値がvalid()を通ることを確認する.
    // lens_shadingのgains要素数は grid_width * grid_height * channels、つまり 2 * 2 * 3 = 12 でなければならない.
    sCameraCalibration calibration;
    calibration.camera_id = "camera-identity";
    calibration.intrinsics = sCameraIntrinsics{
        1920U, 1080U, 1000.0, 1001.0, 959.5, 539.5, 0.0 };
    calibration.distortion = sCameraDistortion{};
    calibration.color = sCameraColorCalibration{};
    calibration.lens_shading = sCameraLensShadingMap{
        2U, 2U, 3U, std::vector< float >( 12U, 1.0F ) };
    expect( calibration.valid(), "Complete calibration value is valid" );

    // CalibrationはOptionalとVectorを値で持つ. Copyが元と独立に有効であることが、
    // 以降の破壊検査をこのCopyから毎回巻き戻せることの前提になる.
    const sCameraCalibration copy = calibration;
    expect( copy.valid() && copy.camera_id == calibration.camera_id,
        "Calibration is an owned copyable value" );

    // ここからはDomainごとに1箇所だけ壊し、そのつどcopyへ巻き戻す.
    // 1つのDomainの不正値が全体を無効化すること、つまりvalid()が持っているDomainを
    // すべて検査していることを固定する. 破壊内容はそれぞれ下流で別の壊れ方をする:
    // 焦点距離0はPinhole modelを退化させ、非有限のDistortion係数はUndistort計算へNaNを伝播させ、
    // 長さの足りないLens shading mapはGain参照を範囲外へ導く.
    calibration.intrinsics->focal_length_x = 0.0;
    expect( !calibration.valid(), "Non-positive focal length is rejected" );
    calibration = copy;
    calibration.distortion->radial[ 0U ] = std::numeric_limits< double >::infinity();
    expect( !calibration.valid(), "Non-finite distortion coefficient is rejected" );
    calibration = copy;
    calibration.lens_shading->gains.pop_back();
    expect( !calibration.valid(), "Truncated lens-shading map is rejected" );

    // camera_idだけを持つ値は「Cameraを特定しただけで、まだ何も較正していない」状態である.
    // これをvalidとすると、中身の無いCalibrationが較正済みとして保存や配布へ回る.
    sCameraCalibration empty;
    empty.camera_id = "camera-identity";
    expect( !empty.valid(), "Calibration without any calibration domain is rejected" );

    return failures == 0 ? 0 : 1;
}
