cmake_minimum_required(VERSION 3.24)
project(SF3GameClang LANGUAGES C)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR NOT MSVC)
    message(FATAL_ERROR "The UWP game library must be built with clang-cl.")
endif()

foreach(required_var IN ITEMS SF3_ROOT SF3_UWP_DEPS_PREFIX SF3_UWP_SDL3_ROOT)
    if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
        message(FATAL_ERROR "Missing required cache variable: ${required_var}")
    endif()
    file(TO_CMAKE_PATH "${${required_var}}" normalized_path)
    set(${required_var} "${normalized_path}")
endforeach()

file(GLOB_RECURSE SF3_GAME_SOURCES CONFIGURE_DEPENDS "${SF3_ROOT}/src/*.c")
list(FILTER SF3_GAME_SOURCES EXCLUDE REGEX "[/\\\\]src[/\\\\]arcade[/\\\\]rom_load\\.c$")

add_library(sf3_game STATIC ${SF3_GAME_SOURCES})
target_include_directories(sf3_game PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../compat/include"
    "${SF3_ROOT}/src"
    "${SF3_ROOT}/include"
    "${SF3_ROOT}/include/sdk"
    "${SF3_UWP_SDL3_ROOT}/include"
    "${SF3_UWP_DEPS_PREFIX}/include"
)
target_compile_definitions(sf3_game PRIVATE
    SF3_UWP=1
    main=SF3_Main
    _CRT_SECURE_NO_WARNINGS
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:RELEASE>
)
target_compile_options(sf3_game PRIVATE
    /W3
    /clang:-std=gnu17
    /clang:-Wno-deprecated-non-prototype
    /clang:-Wno-incompatible-pointer-types
    /clang:-Wno-pointer-sign
    "/FI${CMAKE_CURRENT_LIST_DIR}/../compat/msvc_c_compat.h"
)
set_target_properties(sf3_game PROPERTIES
    OUTPUT_NAME "sf3_game"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
