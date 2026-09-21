/* Compile the complete public layout as C, independently of the C++ bridge. */
#include <wse/capi/wse_capi.h>
#include <wse/capi/wse_capi_core.h>
#include <wse/capi/wse_capi_iui.h>
#include <wse/capi/wse_capi_oui.h>
#include <wse/capi/wse_capi_xpt.h>
#include <wse/capi/wse_capi_tmr.h>
#include <stddef.h>
#include <stdio.h>

#if UINTPTR_MAX == UINT64_MAX
#define CAPI_TYPE(type, managed, size, alignment) \
    _Static_assert(sizeof(type) == size && _Alignof(type) == alignment, #type " layout");
#define CAPI_FIELD(type, field, managed, offset, size) \
    _Static_assert(offsetof(type, field) == offset && sizeof(((type*)0)->field) == size, #type "." #field);
#include "../support/CapiLayout.def"
#undef CAPI_FIELD
#undef CAPI_TYPE
#endif

int main(void)
{
#define CAPI_TYPE(type, managed, size, alignment) \
    printf("TYPE %s %zu %zu\n", #managed, sizeof(type), (size_t)_Alignof(type));
#define CAPI_FIELD(type, field, managed, offset, size) \
    printf("FIELD %s %zu %zu\n", #managed, offsetof(type, field), sizeof(((type*)0)->field));
#include "../support/CapiLayout.def"
#undef CAPI_FIELD
#undef CAPI_TYPE
    return 0;
}
