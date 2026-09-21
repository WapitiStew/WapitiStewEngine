// @file engine/wse/test/characterization/wse_capi_abi_contract.cpp
// @brief 平坦C ABIのSnapshotをCompile時に固定する。
// @details doc/design/en/VersionCompatibility.md が約束するC ABIの安定境界 — `WSE_CAPI_ABI_VERSION`、
//          値渡し`wse_capi_status`のLayout、固定幅Boolean、Opaque handle、Portable分類の数値 — を
//          static_assertで写し取る. ここにある数値のどれかを変える変更はABI破壊であり、
//          `WSE_CAPI_ABI_VERSION`の増加と本Fileの意図的な更新を同時に要求する.
//          実行時の関数呼出は行わない. C ABI Libraryは.NET Binding有効時にだけBuildされるため、
//          この契約はHeaderだけで検証できる形に保っている.

// Core-only Buildでも検証できるよう、Import宣言のまま含める (関数は参照しない).
#include <wse/capi/wse_capi.h>
#include <wse/capi/wse_capi_core.h>
#include <wse/capi/wse_capi_iui.h>
#include <wse/capi/wse_capi_oui.h>
#include <wse/capi/wse_capi_xpt.h>
#include <wse/capi/wse_capi_tmr.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace
{
#if UINTPTR_MAX == UINT64_MAX
#define CAPI_TYPE(type, managed, size, alignment) \
    static_assert(sizeof(type) == size && alignof(type) == alignment, #type " layout");
#define CAPI_FIELD(type, field, managed, offset, size) \
    static_assert(offsetof(type, field) == offset && sizeof(((type*)nullptr)->field) == size, #type "." #field);
#include "../support/CapiLayout.def"
#undef CAPI_FIELD
#undef CAPI_TYPE
#endif
	// --- ABI版. これが変わる変更はこのFile全体の意図的な更新を伴う.
	static_assert(WSE_CAPI_ABI_VERSION == 1U, "flat C ABI version snapshot");

	// --- 固定幅Boolean.
	static_assert(sizeof(wse_capi_bool) == 4, "wse_capi_bool is a 32-bit value");
	static_assert(std::is_same<wse_capi_bool, std::int32_t>::value, "wse_capi_bool type");

	// --- 全関数が値で返すStatusのLayout. FieldのSize・Offset・全体Sizeを固定する.
	static_assert(sizeof(wse_capi_status) == 16, "wse_capi_status size");
	static_assert(offsetof(wse_capi_status, category) == 0, "status category offset");
	static_assert(offsetof(wse_capi_status, code) == 4, "status code offset");
	static_assert(offsetof(wse_capi_status, native_code) == 8, "status native_code offset");
	static_assert(std::is_same<decltype(wse_capi_status::category), std::int32_t>::value,
		"status category type");
	static_assert(std::is_same<decltype(wse_capi_status::code), std::int32_t>::value,
		"status code type");
	static_assert(std::is_same<decltype(wse_capi_status::native_code), std::int64_t>::value,
		"status native_code type");

	// --- Portable分類の数値. `wse::binding::eErrorCategory`と同値である約束ごと写し取る.
	static_assert(WSE_CAPI_ERROR_NONE == 0, "error category NONE");
	static_assert(WSE_CAPI_ERROR_INVALID_ARGUMENT == 1, "error category INVALID_ARGUMENT");
	static_assert(WSE_CAPI_ERROR_NOT_FOUND == 2, "error category NOT_FOUND");
	static_assert(WSE_CAPI_ERROR_INVALID_STATE == 3, "error category INVALID_STATE");
	static_assert(WSE_CAPI_ERROR_INPUT_OUTPUT == 4, "error category INPUT_OUTPUT");
	static_assert(WSE_CAPI_ERROR_TIMEOUT == 5, "error category TIMEOUT");
	static_assert(WSE_CAPI_ERROR_CANCELLATION == 6, "error category CANCELLATION");
	static_assert(WSE_CAPI_ERROR_PROTOCOL == 7, "error category PROTOCOL");
	static_assert(WSE_CAPI_ERROR_SECURITY == 8, "error category SECURITY");
	static_assert(WSE_CAPI_ERROR_UNSUPPORTED == 9, "error category UNSUPPORTED");
	static_assert(WSE_CAPI_ERROR_RESOURCE_EXHAUSTED == 10, "error category RESOURCE_EXHAUSTED");
	static_assert(WSE_CAPI_ERROR_INTERNAL == 11, "error category INTERNAL");

	// --- Opaque handle. 中身へ触れられないPointerであることを固定する.
	static_assert(std::is_pointer<wse_capi_runtime>::value, "runtime handle is opaque");
	static_assert(std::is_pointer<wse_capi_frame_buffer>::value, "frame-buffer handle is opaque");
	static_assert(std::is_pointer<wse_capi_cancellation>::value, "cancellation handle is opaque");
}

int main()
{
	// Compile時のstatic_assertがこのTestの本体であり、実行に至った時点で契約は成立している.
	std::cout << "flat C ABI snapshot holds for WSE_CAPI_ABI_VERSION "
		<< WSE_CAPI_ABI_VERSION << '\n';
	return 0;
}
