cmake_minimum_required(VERSION 3.24)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    message(FATAL_ERROR "Configure this port with CMAKE_SYSTEM_NAME=WindowsStore.")
endif()

project(SF3UWP LANGUAGES C CXX)

set(SF3_VERSION "1.0.0" CACHE STRING "3SXW release version")
if(NOT SF3_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "SF3_VERSION must use X.Y.Z format.")
endif()
string(REPLACE "." ";" SF3_VERSION_PARTS "${SF3_VERSION}")
foreach(part IN LISTS SF3_VERSION_PARTS)
    if(part GREATER 65535)
        message(FATAL_ERROR "MSIX version components cannot exceed 65535.")
    endif()
endforeach()
set(SF3_UWP_PACKAGE_VERSION "${SF3_VERSION}.0")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
    message(FATAL_ERROR "The native UWP host currently requires the MSVC toolchain.")
endif()

set(SF3_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
get_filename_component(SF3_ROOT "${SF3_ROOT}" ABSOLUTE)
set(SF3_UWP_SDL3_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/SDL3")
set(SF3_UWP_SDL_MAIN_RC "${SF3_UWP_SDL3_ROOT}/src/main/winrt/SDL3-WinRTResources.rc")

# Build SDL3 with the app CRT.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_UNINSTALL OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_DISKAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_DUMMYAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_DUMMYVIDEO OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_OFFSCREEN OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_D3D11 ON CACHE BOOL "" FORCE)
set(SDL_RENDER_D3D12 OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
set(SDL_VIRTUAL_JOYSTICK OFF CACHE BOOL "" FORCE)
add_subdirectory("${SF3_UWP_SDL3_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/sdl3" EXCLUDE_FROM_ALL)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/D3D11Shaders.cmake")

find_package(ZLIB REQUIRED)
find_package(MbedTLS CONFIG REQUIRED)

# FindFFMPEG cannot locate Windows SDK libraries in a WindowsStore build.
set(SF3_UWP_VCPKG_PREFIX "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
find_program(SF3_UWP_CLANG_CL NAMES clang-cl REQUIRED)
find_program(SF3_UWP_NINJA NAMES ninja REQUIRED)
include(ExternalProject)

set(SF3_UWP_GAME_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/game-clang")
set(SF3_UWP_GAME_LIBRARY "${SF3_UWP_GAME_BINARY_DIR}/lib/sf3_game.lib")
ExternalProject_Add(SF3GameClangBuild
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/game"
    BINARY_DIR "${SF3_UWP_GAME_BINARY_DIR}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_SYSTEM_NAME=WindowsStore"
        "-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}"
        "-DCMAKE_C_COMPILER:FILEPATH=${SF3_UWP_CLANG_CL}"
        "-DCMAKE_MAKE_PROGRAM:FILEPATH=${SF3_UWP_NINJA}"
        "-DCMAKE_BUILD_TYPE=$<CONFIG>"
        "-DSF3_ROOT:PATH=${SF3_ROOT}"
        "-DSF3_UWP_SDL3_ROOT:PATH=${SF3_UWP_SDL3_ROOT}"
        "-DSF3_UWP_DEPS_PREFIX:PATH=${SF3_UWP_VCPKG_PREFIX}"
    BUILD_BYPRODUCTS "${SF3_UWP_GAME_LIBRARY}"
    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ""
)
add_library(SF3GameClang STATIC IMPORTED GLOBAL)
set_target_properties(SF3GameClang PROPERTIES IMPORTED_LOCATION "${SF3_UWP_GAME_LIBRARY}")
add_dependencies(SF3GameClang SF3GameClangBuild)

set(SF3_UWP_HOST "${CMAKE_CURRENT_LIST_DIR}/compat/sdl_winrt_main.cpp")
set(SF3_UWP_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/Package.appxmanifest")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/Package.appxmanifest.in"
    "${SF3_UWP_MANIFEST}"
    @ONLY
)
set(SF3_UWP_ASSETS
    "${CMAKE_CURRENT_LIST_DIR}/Assets/StoreLogo.png"
    "${CMAKE_CURRENT_LIST_DIR}/Assets/Square44x44Logo.png"
    "${CMAKE_CURRENT_LIST_DIR}/Assets/Square150x150Logo.png"
    "${CMAKE_CURRENT_LIST_DIR}/Assets/SmallTile.png"
    "${CMAKE_CURRENT_LIST_DIR}/Assets/LargeTile.png"
    "${CMAKE_CURRENT_LIST_DIR}/Assets/WideTile.png"
    "${CMAKE_CURRENT_LIST_DIR}/Assets/SplashScreen.png"
)
set(SF3_UWP_BEZELS
    "${SF3_ROOT}/img/16-9/bezel.png"
    "${SF3_ROOT}/img/16-9/bezel-pixel-perfect.png"
    "${SF3_ROOT}/img/16-9/bezel2.png"
    "${SF3_ROOT}/img/16-9/bezel3.png"
)

set(SF3_UWP_NOTICES
    "${SF3_ROOT}/LICENSE"
    "${SF3_ROOT}/docs/THIRD_PARTY_NOTICES.txt"
    "${SF3_UWP_SDL3_ROOT}/LICENSE.txt"
)
set_source_files_properties(${SF3_UWP_NOTICES} PROPERTIES
    VS_DEPLOYMENT_CONTENT 1
    VS_DEPLOYMENT_LOCATION "licenses"
)

add_executable(SF3UWP WIN32
    "${SF3_UWP_HOST}"
    "${SF3_UWP_SDL_MAIN_RC}"
    "${SF3_UWP_MANIFEST}"
    ${SF3_UWP_ASSETS}
    ${SF3_UWP_BEZELS}
    ${SF3_UWP_NOTICES}
)

set_source_files_properties("${SF3_UWP_HOST}" PROPERTIES COMPILE_OPTIONS "/ZW")
set_source_files_properties("${SF3_UWP_MANIFEST}" PROPERTIES
    VS_TOOL_OVERRIDE "AppxManifest"
    VS_COPY_TO_OUT_DIR Always
    VS_DEPLOYMENT_CONTENT 1
    VS_DEPLOYMENT_LOCATION "."
)
set_source_files_properties(${SF3_UWP_ASSETS} PROPERTIES
    VS_TOOL_OVERRIDE "Image"
    VS_COPY_TO_OUT_DIR Always
    VS_DEPLOYMENT_CONTENT 1
    VS_DEPLOYMENT_LOCATION "Assets"
)
set_source_files_properties(${SF3_UWP_BEZELS} PROPERTIES
    VS_COPY_TO_OUT_DIR Always
    VS_DEPLOYMENT_CONTENT 1
    VS_DEPLOYMENT_LOCATION "data/img"
)

target_link_directories(SF3UWP PRIVATE "${SF3_UWP_VCPKG_PREFIX}/lib")
target_link_options(SF3UWP PRIVATE
    /nodefaultlib:vccorlib
    /nodefaultlib:msvcrt
    vccorlib.lib
    msvcrt.lib
)

target_link_libraries(SF3UWP PRIVATE
    SF3GameClang
    SDL3::SDL3-static
    avcodec
    swresample
    avutil
    MbedTLS::mbedcrypto
    ZLIB::ZLIB
    mfuuid
    strmiids
)

set_target_properties(SF3UWP PROPERTIES
    OUTPUT_NAME "SF3"
    VS_GLOBAL_RootNamespace "SF3UWP"
    VS_SDK_REFERENCES "Microsoft.VCLibs, Version=14.0"
)
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT SF3UWP)
