# This port targets Xbox One's D3D11 feature level 10_1. Generate SM4 bytecode
# locally instead of embedding SDL's precompiled ps_5_0 advanced shader.
if(NOT TARGET SDL3-static)
    message(FATAL_ERROR "The UWP shader configuration requires SDL3-static.")
endif()

# FXC is a host tool from the Windows SDK, not a UWP executable.
get_filename_component(SF3_UWP_RC_DIRECTORY "${CMAKE_RC_COMPILER}" DIRECTORY)
file(GLOB SF3_UWP_FXC_SDK_DIRECTORIES LIST_DIRECTORIES TRUE
    "$ENV{WindowsSdkDir}/bin/*/x64"
    "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/*/x64"
)
list(SORT SF3_UWP_FXC_SDK_DIRECTORIES COMPARE NATURAL ORDER DESCENDING)
find_program(SF3_UWP_FXC NAMES fxc.exe
    HINTS
        "${SF3_UWP_RC_DIRECTORY}"
        "$ENV{WindowsSdkVerBinPath}/x64"
        "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
        "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
        ${SF3_UWP_FXC_SDK_DIRECTORIES}
    DOC "Host FXC compiler used to generate Xbox One D3D11 ps_4_0 bytecode"
    NO_CMAKE_FIND_ROOT_PATH
)
if(NOT SF3_UWP_FXC OR NOT EXISTS "${SF3_UWP_FXC}")
    message(FATAL_ERROR
        "fxc.exe was not found. Install the Windows SDK shader compiler or set "
        "SF3_UWP_FXC to its absolute path. The Xbox One UWP port requires "
        "generated ps_4_0 bytecode and will not fall back to ps_5_0."
    )
endif()

set(SF3_UWP_D3D11_SHADER_SOURCE_DIR "${SF3_UWP_SDL3_ROOT}/src/render/direct3d11")
set(SF3_UWP_D3D11_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders/d3d11")
set(SF3_UWP_D3D11_SHADER_HEADER
    "${SF3_UWP_D3D11_SHADER_OUTPUT_DIR}/SF3_D3D11_PixelShader_Advanced_SM4.h"
)

add_custom_command(
    OUTPUT "${SF3_UWP_D3D11_SHADER_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SF3_UWP_D3D11_SHADER_OUTPUT_DIR}"
    COMMAND "${SF3_UWP_FXC}"
        /nologo /T ps_4_0 /E main /Vn g_main
        /I "${SF3_UWP_D3D11_SHADER_SOURCE_DIR}"
        /Fh "${SF3_UWP_D3D11_SHADER_HEADER}"
        "${SF3_UWP_D3D11_SHADER_SOURCE_DIR}/D3D11_PixelShader_Advanced.hlsl"
    DEPENDS
        "${SF3_UWP_FXC}"
        "${SF3_UWP_D3D11_SHADER_SOURCE_DIR}/D3D11_PixelShader_Advanced.hlsl"
        "${SF3_UWP_D3D11_SHADER_SOURCE_DIR}/D3D11_PixelShader_Common.hlsli"
        "${CMAKE_CURRENT_LIST_FILE}"
    COMMENT "Generating Xbox One D3D11 advanced shader (ps_4_0)"
    VERBATIM
)
add_custom_target(SF3UWPShaders DEPENDS "${SF3_UWP_D3D11_SHADER_HEADER}")
add_dependencies(SDL3-static SF3UWPShaders)
set_source_files_properties("${SF3_UWP_D3D11_SHADER_HEADER}" PROPERTIES
    GENERATED TRUE
    HEADER_FILE_ONLY TRUE
)
target_sources(SDL3-static PRIVATE "${SF3_UWP_D3D11_SHADER_HEADER}")
target_include_directories(SDL3-static PRIVATE "${SF3_UWP_D3D11_SHADER_OUTPUT_DIR}")
target_compile_definitions(SDL3-static PRIVATE SF3_UWP_D3D11_ADVANCED_SM4=1)
message(STATUS "Xbox One UWP: D3D11 advanced shader profile ps_4_0 (FXC: ${SF3_UWP_FXC})")
