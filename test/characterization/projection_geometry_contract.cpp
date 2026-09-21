// @file engine/wse/test/characterization/projection_geometry_contract.cpp
// @brief Projectionで使うHomographyとMeshのCPU幾何学契約を固定する。
// @brief この契約が破れると、Projection warpを使うApplicationが次を失う.
//        1) 四点／二点対応のHomographyが対応点を正確に写すこと.
//        2) 三点構築がUNIMPLEMENTEDとして明示的に拒否されること.
//        3) LegacyのPixel座標MeshがPortableのNDC／UVへ正しく正規化されること.
//        4) 有限な画面外座標がWarpとして通り、Clip／Clampで観測可能なままであること.
//        GPUは一切使わない。ここで固定するのはCPU側の幾何計算だけであり、実際の描画結果は
//        oui_d3d12系のGolden testが受け持つ.

#include <wse/stew.h>

#include <stdexcept>
#include <oui/renderer/ProjectionMeshAdapter.h>

#include <cmath>
#include <cstddef>
#include <iostream>
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

	void expectPoint(
		const wse::float64_xy& actual_in,
		const wse::float64_xy& expected_in,
		const char* const message_in)
	{
		expect(near(actual_in.x, expected_in.x) && near(actual_in.y, expected_in.y), message_in);
	}
}

int main()
{
	const std::vector<wse::float64_xy> sourceCorners = {
		{0.0, 0.0}, {100.0, 0.0}, {100.0, 100.0}, {0.0, 100.0}
	};
	const std::vector<wse::float64_xy> destinationCorners = {
		{10.0, 20.0}, {210.0, 20.0}, {210.0, 120.0}, {10.0, 120.0}
	};
	const wse::Homography fourPoint(sourceCorners, destinationCorners);
	for (std::size_t index = 0; index < sourceCorners.size(); ++index)
	{
		expectPoint(
			fourPoint.transform(sourceCorners[index]),
			destinationCorners[index],
			"Four-point Homography corner mapping");
	}
	expectPoint(
		fourPoint.transform(wse::float64_xy(50.0, 50.0)),
		wse::float64_xy(110.0, 70.0),
		"Four-point Homography center mapping");

	const std::vector<wse::float64_xy> sourceDiagonal = {
		{10.0, 20.0}, {110.0, 70.0}
	};
	const std::vector<wse::float64_xy> destinationDiagonal = {
		{0.0, 0.0}, {200.0, 100.0}
	};
	const wse::Homography twoPoint(sourceDiagonal, destinationDiagonal);
	expectPoint(
		twoPoint.transform(wse::float64_xy(60.0, 45.0)),
		wse::float64_xy(100.0, 50.0),
		"Two-point rectangle mapping");

	bool rejectedThreePoints = false;
	try
	{
		const std::vector<wse::float64_xy> threePoints = {
			{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}
		};
		const wse::Homography unsupported(threePoints, threePoints);
		(void)unsupported;
	}
	catch (const std::invalid_argument&)
	{
		rejectedThreePoints = true;
	}
	expect(rejectedThreePoints, "Three-point Homography is explicitly unsupported");

	wse::float64_mesh2d mesh(2, 3);
	for (std::size_t row = 0; row < mesh.vertex_height(); ++row)
	{
		for (std::size_t column = 0; column < mesh.vertex_width(); ++column)
		{
			const double value = static_cast<double>((row * mesh.vertex_width()) + column);
			mesh[row][column].src = wse::float64_xy(value, value + 10.0);
			mesh[row][column].dst = wse::float64_xy(value + 20.0, value + 30.0);
		}
	}
	expect(mesh.vertex_width() == 3 && mesh.vertex_height() == 2, "Rectangular mesh vertices");
	expect(mesh.mesh_width() == 2 && mesh.mesh_height() == 1, "Rectangular mesh cells");
	const std::vector<wse::float64_xy> sourceList = mesh.getSrcList();
	const std::vector<wse::float64_xy> destinationList = mesh.getDstList();
	expect(sourceList.size() == 6, "Rectangular mesh source list size");
	expect(destinationList.size() == 6, "Rectangular mesh destination list size");
	expectPoint(sourceList.back(), wse::float64_xy(5.0, 15.0), "Mesh source row-major order");
	expectPoint(destinationList.back(), wse::float64_xy(25.0, 35.0), "Mesh destination row-major order");

	const auto grid = mesh.getGrid(0, 1);
	expect(
		grid.index[wse::MESH_LEFT_TOP] == 1 &&
		grid.index[wse::MESH_RIGHT_TOP] == 2 &&
		grid.index[wse::MESH_RIGHT_BOTTOM] == 5 &&
		grid.index[wse::MESH_LEFT_BOTTOM] == 4,
		"Mesh grid indices");

	const auto portableMesh = wse::oui::ProjectionMeshAdapter::createMesh(
		mesh, {101U, 101U}, {101U, 101U});
	expect(portableMesh.succeeded(), "Legacy projection mesh converts to portable mesh");
	if (portableMesh.succeeded())
	{
		expect(portableMesh.value().vertices.size() == 6U, "Portable mesh vertex count");
		expect(portableMesh.value().indices.size() == 12U, "Portable mesh index count");
		expect(
			near(portableMesh.value().vertices.front().position_x, -0.6) &&
			near(portableMesh.value().vertices.front().position_y, 0.4) &&
			near(portableMesh.value().vertices.front().texture_u, 0.0) &&
			near(portableMesh.value().vertices.front().texture_v, 0.1),
			"Legacy pixel coordinates normalize to portable NDC and UV");
		expect(
			portableMesh.value().indices[0U] == 0U &&
			portableMesh.value().indices[1U] == 1U &&
			portableMesh.value().indices[2U] == 4U &&
			portableMesh.value().indices[5U] == 3U,
			"Legacy grid winding converts to portable triangles");
	}

	// Source／Targetが異なる解像度でも各終端PixelがUV／NDC終端へ一致する.
	wse::float64_mesh2d differentExtentMesh(2, 2);
	differentExtentMesh[0][0].src = {0.0, 0.0};
	differentExtentMesh[0][1].src = {1919.0, 0.0};
	differentExtentMesh[1][0].src = {0.0, 1079.0};
	differentExtentMesh[1][1].src = {1919.0, 1079.0};
	differentExtentMesh[0][0].dst = {0.0, 0.0};
	differentExtentMesh[0][1].dst = {1279.0, 0.0};
	differentExtentMesh[1][0].dst = {0.0, 719.0};
	differentExtentMesh[1][1].dst = {1279.0, 719.0};
	const auto differentExtentResult = wse::oui::ProjectionMeshAdapter::createMesh(
		differentExtentMesh, {1280U, 720U}, {1920U, 1080U});
	expect(differentExtentResult.succeeded(), "Different source and target extents convert");
	if (differentExtentResult.succeeded())
	{
		const auto& first = differentExtentResult.value().vertices.front();
		const auto& last = differentExtentResult.value().vertices.back();
		expect(
			near(first.position_x, -1.0) && near(first.position_y, 1.0) &&
			near(first.texture_u, 0.0) && near(first.texture_v, 0.0) &&
			near(last.position_x, 1.0) && near(last.position_y, -1.0) &&
			near(last.texture_u, 1.0) && near(last.texture_v, 1.0),
			"Different extents preserve endpoint normalization");
	}

	// 有限な画面外座標はProjection warpとして許可し、Rasterizer clip／Sampler clampへ渡す.
	wse::float64_mesh2d extremeMesh = differentExtentMesh;
	extremeMesh[0][0].src = {-1919.0, -1079.0};
	extremeMesh[1][1].src = {3838.0, 2158.0};
	extremeMesh[0][0].dst = {-1279.0, -719.0};
	extremeMesh[1][1].dst = {2558.0, 1438.0};
	const auto extremeResult = wse::oui::ProjectionMeshAdapter::createMesh(
		extremeMesh, {1280U, 720U}, {1920U, 1080U});
	expect(extremeResult.succeeded(), "Finite out-of-bounds projection mesh converts");
	if (extremeResult.succeeded())
	{
		const auto& first = extremeResult.value().vertices.front();
		const auto& last = extremeResult.value().vertices.back();
		expect(
			first.position_x < -1.0F && first.position_y > 1.0F &&
			first.texture_u < 0.0F && first.texture_v < 0.0F &&
			last.position_x > 1.0F && last.position_y < -1.0F &&
			last.texture_u > 1.0F && last.texture_v > 1.0F,
			"Extreme finite coordinates remain observable for clipping and clamping");
	}

	const wse::float64_mesh2d emptyMesh;
	// OUI-PROJ-01: non-square extents and an interior column expose width/height
	// swaps and W versus W-1 normalization; verify both grid cells' exact winding.
	wse::float64_mesh2d referenceMesh(2, 3);
	for (std::size_t row = 0; row < 2; ++row)
	{
		for (std::size_t column = 0; column < 3; ++column)
		{
			referenceMesh[row][column].src = {double(column * 4), double(row * 4)};
			referenceMesh[row][column].dst = {double(column * 2), double(row * 2)};
		}
	}
	const auto referenceResult = wse::oui::ProjectionMeshAdapter::createMesh(
		referenceMesh, {5U, 3U}, {9U, 5U});
	expect(referenceResult.succeeded(), "Rectangular reference grid converts");
	if (referenceResult.succeeded())
	{
		const auto& centerTop = referenceResult.value().vertices[1];
		expect(near(centerTop.position_x, 0.0) && near(centerTop.position_y, 1.0) &&
			near(centerTop.texture_u, 0.5) && near(centerTop.texture_v, 0.0),
			"Rectangular interior vertex matches the documented NDC/UV vector");
		expect(referenceResult.value().indices ==
			std::vector<std::uint32_t>({0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4}),
			"Both rectangular grid cells retain row-major triangle order");
	}
	expect(emptyMesh.mesh_width() == 0 && emptyMesh.mesh_height() == 0, "Empty mesh dimensions");
	expect(
		!wse::oui::ProjectionMeshAdapter::createMesh(emptyMesh, {2U, 2U}, {2U, 2U}).succeeded(),
		"Empty legacy projection mesh is rejected");

	return failures == 0 ? 0 : 1;
}
