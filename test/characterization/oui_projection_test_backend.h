//*****************************************************************************************************************
//!
//! @file    oui_projection_test_backend.h
//! @brief   \~japanese Projection Characterization TestのBackend選択を共通化する.
//! @brief   \~english  Shares backend selection for projection characterization tests.
//! @author  WapitiStew.
//! @par     Character Code: UTF-8N
//! @par     Return Code: LF
//!
//! @date
//!   Aug-28, 2026   Create New.
//!
//! @copyright
//!   Copyright (C) 2026 WapitiStew. All rights reserved.
//*****************************************************************************************************************

#ifndef WSE_TEST_CHARACTERIZATION_OUI_PROJECTION_TEST_BACKEND_H
#define WSE_TEST_CHARACTERIZATION_OUI_PROJECTION_TEST_BACKEND_H

#include <oui/renderer/Renderer.h>

#include <string>

namespace wse::oui::test
{

//! @brief Command lineからCharacterization Backendを選択する.
//! @param [in]  argument_count_in Argument count.
//! @param [in]  pp_arguments_in   Argument vector.
//! @param [out] p_configuration_out Renderer configuration.
//! @param [out] p_backend_name_out  Stable backend name.
//! @return Backendが有効な場合true.
inline bool configureProjectionBackend(
      sRendererConfiguration* const   p_configuration_out
    , std::string* const              p_backend_name_out
    , const int                       argument_count_in
    , const char* const* const        pp_arguments_in
)
{
    if( argument_count_in != 2 || p_configuration_out == nullptr ||
        p_backend_name_out == nullptr )
        return false;
    *p_backend_name_out = pp_arguments_in[ 1U ];
    p_configuration_out->use_software_adapter = true;
    if( *p_backend_name_out == "d3d12" )
        p_configuration_out->backend = eRendererBackend::Direct3D12;
    else if( *p_backend_name_out == "vulkan" )
        p_configuration_out->backend = eRendererBackend::Vulkan12;
    else
        return false;
    return true;
}

} // namespace wse::oui::test

#endif // WSE_TEST_CHARACTERIZATION_OUI_PROJECTION_TEST_BACKEND_H
