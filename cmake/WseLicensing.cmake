# Package terms cover the public SDK and, when selected, a separately licensed overlay.
set(WSE_PACKAGE_LICENSE_EXPRESSION "Apache-2.0")
set(WSE_PACKAGE_LICENSE_FILES_JSON "\"LICENSE\", \"NOTICE\"")
if(WSE_EXTENSION_ROOT)
    if(NOT WSE_EXTENSION_LICENSE_IDENTIFIER MATCHES "^[A-Za-z0-9][A-Za-z0-9.+-]*$"
            OR NOT WSE_EXTENSION_LICENSE_FILE
            OR NOT EXISTS "${WSE_EXTENSION_LICENSE_FILE}")
        message(FATAL_ERROR "An extension package requires its own license identifier and license file.")
    endif()
    set(WSE_PACKAGE_LICENSE_EXPRESSION "Apache-2.0 AND ${WSE_EXTENSION_LICENSE_IDENTIFIER}")
    string(APPEND WSE_PACKAGE_LICENSE_FILES_JSON ", \"LICENSE.extension\"")
    install(FILES "${WSE_EXTENSION_LICENSE_FILE}" DESTINATION "." RENAME LICENSE.extension)
endif()
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE"
    DESTINATION ".")
