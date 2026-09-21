// SPDX-License-Identifier: Apache-2.0
// Explicit instantiations need dllexport on Windows. On ELF, the template's
// declaration (or default visibility) already sets visibility; a second type
// attribute on an instantiated specialization is ignored by GCC.
#ifndef WSE_INTERNAL_TEMPLATE_EXPORT_H
#define WSE_INTERNAL_TEMPLATE_EXPORT_H

#if defined(_WIN32)
#define WSE_INSTANTIATION_API WSE_API
#else
#define WSE_INSTANTIATION_API
#endif

#endif
