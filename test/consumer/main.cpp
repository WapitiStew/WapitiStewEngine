// @file engine/wse/test/consumer/main.cpp
// @brief Installed WSE packageのLinkとRuntime Loadを確認する最小Consumer。
// @brief この検査が破れると、Install済みPackageからWSEを使う外部Projectが次を失う.
//        1) find_packageで選んだComponent構成のHeaderが揃い、Linkが成立すること.
//        2) 各Componentの代表APIが実行時に呼べること（DLLのLoadと輸出Symbolの実在）.
//        Hardwareに触らない検査だけを置く: Deviceを開かず、失敗して当然の呼び出しには
//        NotInitialized等の構造化Errorが返ることを確認へ使う。どのComponentを検査するかは
//        WSE_CONSUMER_INCLUDE_*のCompile定義が決め、Package gateが構成別にBuildして回す。
//        戻り値はComponentごとに番号を分け、どの面が壊れたかをExit codeだけで指せる.

#include <wse/stew.h>
#include "ApplicationLog.h"
#include "../support/FastIndexingContract.h"

#include <cstdint>
#include <vector>

#if defined(WSE_CONSUMER_INCLUDE_ALL_COMPONENTS) || \
	defined(WSE_CONSUMER_INCLUDE_PUBLIC_COMPONENTS) || defined(WSE_CONSUMER_INCLUDE_XPT)
#include <xpt/stew.h>
#endif

#if defined(WSE_CONSUMER_INCLUDE_ALL_COMPONENTS) || \
	defined(WSE_CONSUMER_INCLUDE_PUBLIC_COMPONENTS) || defined(WSE_CONSUMER_INCLUDE_GEF)
#include <gef/stew.h>
#endif

#if defined(WSE_CONSUMER_INCLUDE_ALL_COMPONENTS) || \
	defined(WSE_CONSUMER_INCLUDE_PUBLIC_COMPONENTS)
#include <iui/stew.h>
#include <oui/stew.h>
#include <tmr/stew.h>
#endif

#if defined(WSE_CONSUMER_INCLUDE_IUI)
#include <iui/stew.h>
#endif

#if defined(WSE_CONSUMER_INCLUDE_ALL_COMPONENTS)
#include <vpj/stew.h>
#elif defined(WSE_CONSUMER_INCLUDE_VPJ_CONTROL)
#include <vpj/stew.h>
#elif defined(WSE_CONSUMER_INCLUDE_VPJ_VIDEO)
#include <vpj/stew.h>
#include <vpj/video/stew.h>
#elif defined(WSE_CONSUMER_INCLUDE_OUI)
#include <oui/stew.h>
#endif

#if defined(WSE_CONSUMER_INCLUDE_TMR)
#include <tmr/stew.h>
#endif

int main()
{
	if (!consumer::verifyApplicationLog()) { return 91; }
	try { wse_test::verifyFastIndexing(); }
	catch (const std::exception&) { return 90; }
	const wse::LogLevel originalLevel = wse::minimumLogLevel();
	wse::setMinimumLogLevel(wse::LogLevel::Debug);

	if(wse::minimumLogLevel() != wse::LogLevel::Debug)
	{
		return 1;
	}

	wse::setMinimumLogLevel(originalLevel);

#if defined(WSE_CONSUMER_INCLUDE_VPJ_CONTROL)
	wse::vpj::ModelProfileRegistry profileRegistry;
	const auto unknownModel = profileRegistry.resolveIdentity("consumer.unknown-model");
	if(unknownModel.state != wse::vpj::eModelIdentityState::Unknown ||
		unknownModel.requested_id != "consumer.unknown-model" ||
		!unknownModel.profile_id.empty())
	{
		return 7;
	}
	wse::vpj::Projector projector;
	wse::vpj::sSerialControlOptions serialOptions;
	projector.setVideoType( wse::vpj::eVideo::LAN );
	projector.setVideoMode( wse::vpj::eVideoMode::STILL );
	projector.setStillQuality( 70 );
	if( serialOptions.baud_rate != 9600 || serialOptions.reconnect_attempts != 60U ||
		projector.disconnectControlRS232C() ||
		projector.getVideoType() != wse::vpj::eVideo::LAN ||
		projector.getVideoMode() != wse::vpj::eVideoMode::STILL ||
		projector.getStillQuality() != 70 ||
		projector.connectVideoLAN( "127.0.0.1", true ) )
	{
		return 9;
	}
#endif

#if defined(WSE_CONSUMER_INCLUDE_ALL_COMPONENTS) || \
	defined(WSE_CONSUMER_INCLUDE_PUBLIC_COMPONENTS) || defined(WSE_CONSUMER_INCLUDE_GEF)
	wse::gef::BINController binary;
	binary.setContents(std::vector<std::uint8_t>{1U, 2U, 3U});
	if (binary.index_num() != 1U ||
		binary.contents<std::uint8_t>(0U) != std::vector<std::uint8_t>({1U, 2U, 3U}))
	{
		return 5;
	}
#endif

#if defined(WSE_CONSUMER_INCLUDE_ALL_COMPONENTS) || \
	defined(WSE_CONSUMER_INCLUDE_PUBLIC_COMPONENTS) || defined(WSE_CONSUMER_INCLUDE_IUI)
	wse::iui::Keyboard keyboard;
	if (keyboard.isAvailable() !=
		(keyboard.accessState() == wse::iui::KeyboardAccessState::Ready))
	{
		return 6;
	}
#endif

#if defined(WSE_CONSUMER_INCLUDE_ALL_COMPONENTS) || \
	defined(WSE_CONSUMER_INCLUDE_PUBLIC_COMPONENTS) || defined(WSE_CONSUMER_INCLUDE_XPT)
	const wse::xpt::Endpoint endpoint("127.0.0.1", 1);
	wse::xpt::TcpClient client;
	wse::xpt::UdpClient udpClient;
	wse::xpt::SerialPort serialPort;
	const wse::xpt::RetryPolicy retryPolicy;
	const wse::xpt::HttpClient httpClient;
	const wse::xpt::HttpRequest invalidHttpRequest(
		wse::xpt::eHttpMethod::Get, "invalid-url");
	const auto invalidHttpResult = httpClient.execute(
		invalidHttpRequest,
		wse::xpt::HttpExecutionOptions(),
		wse::xpt::OperationContext(wse::xpt::Timeout::milliseconds(100)));
	if (!endpoint.isValid() || client.isConnected() || udpClient.isOpen() || serialPort.isOpen() ||
		retryPolicy.maximum_attempts() != 1U ||
		invalidHttpResult.succeeded() ||
		invalidHttpResult.error().category() != wse::xpt::eTransportErrorCategory::Validation ||
		wse::xpt::UdpClient::maximumDatagramSize() < 65507)
	{
		return 2;
	}
#endif

#if defined(WSE_CONSUMER_INCLUDE_ALL_COMPONENTS) || \
	defined(WSE_CONSUMER_INCLUDE_PUBLIC_COMPONENTS) || defined(WSE_CONSUMER_INCLUDE_OUI)
	wse::oui::Renderer renderer;
	const auto rendererCapabilities = renderer.getCapabilities();
	const auto rendererDisplays = renderer.enumerateDisplays();
	const auto rendererSurfaceState = renderer.getSurfaceState({1U, 1U});
	const auto rendererSurfaceResize = renderer.resizeSurface({1U, 1U}, {16U, 16U});
	const auto rendererWindowMode = renderer.setSurfaceWindowMode(
		{1U, 1U}, wse::oui::sSurfaceWindowModeRequest{});
	const auto rendererSurfaceEvents = renderer.pollSurfaceEvents({1U, 1U});
	wse::oui::sRendererFrameDescription rendererFrame;
	rendererFrame.extent = {16U, 16U};
	rendererFrame.format = wse::oui::eRendererPixelFormat::Rgba8Unorm;
	wse::oui::sMeshDescription rendererMesh;
	rendererMesh.vertices = {
		{-1.0F, -1.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.5F, 0.0F}, {1.0F, -1.0F, 1.0F, 1.0F}};
	rendererMesh.indices = {0U, 1U, 2U};
	const auto rendererMeshResult = renderer.createMesh(rendererMesh);
	const auto rendererMeshUpdate = renderer.updateMesh({1U, 1U}, rendererMesh);
	wse::oui::sProjectionPassDescription projectionPass;
	projectionPass.color_attachment = {1U, 1U};
	projectionPass.render_area.extent = {16U, 16U};
	projectionPass.supersample_scale = 2U;
	projectionPass.layers.emplace_back(wse::oui::sProjectionLayerDescription{
		{2U, 1U}, {3U, 1U}, {4U, 1U}, wse::oui::eTextureSamplingFilter::Linear, 1.0F,
		{0.25F, 0.0F, 0.0F, 0.25F, wse::oui::eEdgeBlendCurve::Smoothstep}});
	const auto projectionResult = wse::oui::ProjectionPipeline::execute(&renderer, projectionPass);
	if (rendererCapabilities.succeeded() ||
		rendererCapabilities.error().code() != wse::oui::eRendererErrorCode::NotInitialized ||
		rendererDisplays.succeeded() ||
		rendererDisplays.error().code() != wse::oui::eRendererErrorCode::NotInitialized ||
		rendererSurfaceState.succeeded() ||
		rendererSurfaceState.error().code() != wse::oui::eRendererErrorCode::NotInitialized ||
		rendererSurfaceResize.succeeded() ||
		rendererSurfaceResize.error().code() != wse::oui::eRendererErrorCode::NotInitialized ||
		rendererWindowMode.succeeded() ||
		rendererWindowMode.error().code() != wse::oui::eRendererErrorCode::NotInitialized ||
		rendererSurfaceEvents.succeeded() ||
		rendererSurfaceEvents.error().code() != wse::oui::eRendererErrorCode::NotInitialized ||
		!wse::oui::validateRendererFrameDescription(rendererFrame).ok() ||
		rendererFrame.memorySize() != 1024U || rendererMeshResult.succeeded() ||
		rendererMeshResult.error().code() != wse::oui::eRendererErrorCode::NotInitialized ||
		rendererMeshUpdate.succeeded() ||
		rendererMeshUpdate.error().code() != wse::oui::eRendererErrorCode::NotInitialized ||
		!wse::oui::validateProjectionPassDescription(projectionPass).ok() ||
		projectionResult.succeeded() ||
		projectionResult.error().code() != wse::oui::eRendererErrorCode::NotInitialized)
	{
		return 3;
	}
#endif

#if defined(WSE_CONSUMER_INCLUDE_TMR) || defined(WSE_CONSUMER_INCLUDE_PUBLIC_COMPONENTS)
	wse::tmr::WebCamera webCamera;
	const auto webCameraStart = webCamera.start();
	wse::tmr::sCameraCalibration calibration;
	calibration.camera_id = "package-consumer";
	calibration.color = wse::tmr::sCameraColorCalibration{};
	wse::tmr::sCameraControlCapability control;
	control.minimum = 0;
	control.maximum = 100;
	control.step = 5;
	control.default_value = 50;
	control.supports_manual = true;
	wse::tmr::sCameraStreamProfile profile;
	profile.native_format = {1920U, 1080U, 30U, 1U,
		wse::tmr::eCameraPixelFormat::Mjpeg};
	profile.output_formats = {wse::tmr::eCameraPixelFormat::Bgra8};
	wse::tmr::sCameraExtensionUnitSelector selector;
	selector.unit_id = 3U;
	selector.selector = 2U;
	selector.minimum_size = 4U;
	selector.maximum_size = 4U;
	selector.readable = true;
	if (webCamera.isOpen() || webCameraStart.succeeded() ||
		webCameraStart.error().code() != wse::tmr::eCameraErrorCode::NotOpen ||
		!calibration.valid() || control.valueFromNormalized(0.5) != 50 ||
		control.normalizedFromValue(25) != 0.25 ||
		control.physicalFromValue(25) != 25.0 ||
		control.valueFromPhysical(27.0) != 25 || !profile.valid() ||
		!profile.supportsOutput(wse::tmr::eCameraPixelFormat::Bgra8) ||
		!selector.valid())
	{
		return 4;
	}
#endif

#if defined(WSE_CONSUMER_INCLUDE_VPJ_VIDEO) || \
	defined(WSE_CONSUMER_INCLUDE_VPJ_CONTROL) || defined(WSE_CONSUMER_INCLUDE_ALL_COMPONENTS)
	wse::vpj::video::CpuFrame videoFrame;
	videoFrame.width = 2U;
	videoFrame.height = 2U;
	videoFrame.stride = 6U;
	videoFrame.pixelFormat = wse::vpj::video::ePixelFormat::Rgb8;
	videoFrame.pixels = {
		255U, 0U, 0U, 0U, 255U, 0U,
		0U, 0U, 255U, 255U, 255U, 255U};
	wse::vpj::video::ReplayJpegPacketizer videoPacketizer;
	const auto videoPackets = videoPacketizer.framePackets(videoFrame);
	if (!videoPackets.succeeded() || videoPackets.value().size() != 1U ||
		videoPackets.value().front().size() <= 36U)
	{
		return 8;
	}
#endif
	return 0;
}
