cmake_minimum_required(VERSION 3.24)
project(SF3 LANGUAGES C)

# ======================================
# Options and platform setup
# ======================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ======================================
# Include directories
# ======================================

include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/sdk
)

# ======================================
# Source collection
# ======================================

file(GLOB_RECURSE GAME_SRC CONFIGURE_DEPENDS src/*.c)

if(WIN32)
    set(WINDOWS_RESOURCE_FILE "${CMAKE_SOURCE_DIR}/src/sf3.rc")
    set_source_files_properties("${WINDOWS_RESOURCE_FILE}" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/sf3.ico"
    )
    list(APPEND GAME_SRC "${WINDOWS_RESOURCE_FILE}")
endif()

add_executable(SF3 MACOSX_BUNDLE
    ${GAME_SRC}
)

# ======================================
# Compiler and linker flags
# ======================================

target_compile_definitions(SF3 PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Debug>:NETPLAY_ENABLED>

    $<$<CONFIG:Release>:RELEASE>
    $<$<CONFIG:Release>:CHECKSUM>

    $<$<PLATFORM_ID:Darwin>:_DARWIN_C_SOURCE>
    $<$<NOT:$<PLATFORM_ID:Darwin>>:_POSIX_C_SOURCE=200809L>

    # GekkoNet is a pre-built static lib; these were previously propagated by its cmake target
    GEKKONET_STATIC
    GEKKONET_NO_ASIO
)

# Feature toggles
target_compile_definitions(SF3 PRIVATE
    # CPS3
)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    set(DISABLED_WARNINGS
        -Wpointer-to-int-cast
        -Wno-deprecated-non-prototype
        -Wno-tautological-overlap-compare
        -Wno-c2x-extensions
        -Wno-incompatible-pointer-types
        -Wno-pointer-sign
    )
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(DISABLED_WARNINGS
        -Wno-unused-but-set-variable
        -Wno-pointer-sign
        -Wno-parentheses
        -Wno-incompatible-pointer-types
        -Wno-discarded-qualifiers
        -Wno-format-overflow
        -Wno-comment
        -Wno-maybe-uninitialized
        -Wno-array-bounds
        -Wno-aggressive-loop-optimizations
        -Wno-stringop-overflow
        -Wno-restrict
        -Wno-stringop-truncation
    )
endif()

target_compile_options(SF3 PRIVATE
    -Wall
    -Werror

    -fno-strict-aliasing

    ${DISABLED_WARNINGS}

    $<$<CONFIG:Debug>:-Wno-unused-function>
    $<$<CONFIG:Debug>:-Wno-unused-variable>
)

set_target_properties(SF3 PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/Info.plist"
)

if(APPLE)
    set_target_properties(SF3 PROPERTIES
        OUTPUT_NAME "3SX"
        MACOSX_BUNDLE_BUNDLE_NAME "3SX"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
elseif(UNIX AND NOT APPLE)
    set_target_properties(SF3 PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../lib"
    )
endif()

# ======================================
# Dependencies
# ======================================

set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")
find_package(ZLIB REQUIRED)

set(FFMPEG_ROOT        "${THIRD_PARTY_DIR}/ffmpeg/build")
set(SDL3_ROOT          "${THIRD_PARTY_DIR}/sdl3/build")
set(LIBCDIO_ROOT       "${THIRD_PARTY_DIR}/libcdio/build")
set(GEKKONET_ROOT      "${THIRD_PARTY_DIR}/GekkoNet/build")
set(SDL3_NET_ROOT      "${THIRD_PARTY_DIR}/SDL_net/build")
set(MINIZIP_NG_ROOT    "${THIRD_PARTY_DIR}/minizip-ng/build")
set(TF_PSA_CRYPTO_ROOT "${THIRD_PARTY_DIR}/tf-psa-crypto/build")

include_directories(
    ${FFMPEG_ROOT}/include
    ${SDL3_ROOT}/include
    ${LIBCDIO_ROOT}/include
    ${GEKKONET_ROOT}/include
    ${SDL3_NET_ROOT}/include
    ${MINIZIP_NG_ROOT}/include
    ${TF_PSA_CRYPTO_ROOT}/include
)

target_link_libraries(SF3 PRIVATE
    m
    "${LIBCDIO_ROOT}/lib/libiso9660.a"
    "${LIBCDIO_ROOT}/lib/libcdio.a"
    "${GEKKONET_ROOT}/lib/libGekkoNet.a"
    "${SDL3_NET_ROOT}/lib/libSDL3_net.a"
    "${MINIZIP_NG_ROOT}/lib/libminizip-ng.a"
    "${TF_PSA_CRYPTO_ROOT}/lib/libtfpsacrypto.a"
    ZLIB::ZLIB
    stdc++ # For GekkoNet
)

if(APPLE)
    target_link_libraries(SF3 PRIVATE
        ${FFMPEG_ROOT}/lib/libavcodec.dylib
        ${FFMPEG_ROOT}/lib/libavformat.dylib
        ${FFMPEG_ROOT}/lib/libavutil.dylib
        ${FFMPEG_ROOT}/lib/libswresample.dylib
        ${SDL3_ROOT}/lib/libSDL3.0.dylib
        iconv
    )
elseif(WIN32)
    target_link_libraries(SF3 PRIVATE
        ${FFMPEG_ROOT}/lib/libavcodec.dll.a
        ${FFMPEG_ROOT}/lib/libavformat.dll.a
        ${FFMPEG_ROOT}/lib/libavutil.dll.a
        ${FFMPEG_ROOT}/lib/libswresample.dll.a
        ${SDL3_ROOT}/lib/libSDL3.dll.a
        iconv
        bcrypt
		dbghelp
        ws2_32
        iphlpapi
    )
    target_link_options(SF3 PRIVATE
        "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:--for-linker>"
        "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:--pdb=SF3.pdb>"
    )
elseif(UNIX)
    target_link_libraries(SF3 PRIVATE
        ${FFMPEG_ROOT}/lib/libavcodec.so
        ${FFMPEG_ROOT}/lib/libavformat.so
        ${FFMPEG_ROOT}/lib/libavutil.so
        ${FFMPEG_ROOT}/lib/libswresample.so
        ${SDL3_ROOT}/lib/libSDL3.so
    )
endif()

# ======================================
# Installation
# ======================================

set(PROJECT_LICENSE_FILES
    "${CMAKE_SOURCE_DIR}/LICENSE"
    "${CMAKE_SOURCE_DIR}/docs/THIRD_PARTY_NOTICES.txt"
)

if(APPLE)
    install(TARGETS SF3
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )

    file(GLOB FFMPEG_DYLIBS "${FFMPEG_ROOT}/lib/*.dylib")

    install(FILES
        ${FFMPEG_DYLIBS}
        ${SDL3_ROOT}/lib/libSDL3.0.dylib
        DESTINATION 3SX.app/Contents/Frameworks
    )

    install(FILES
        ${PROJECT_LICENSE_FILES}
        DESTINATION 3SX.app/Contents/Resources/licenses
    )
elseif(WIN32)
    install(TARGETS SF3
        RUNTIME DESTINATION bin
    )

    if(DEFINED MSYS2_LOCATION AND NOT MSYS2_LOCATION STREQUAL "")
        file(TO_CMAKE_PATH "${MSYS2_LOCATION}/mingw64/bin" binPath)
    else()
        get_filename_component(binPath "${CMAKE_C_COMPILER}" DIRECTORY)
    endif()

    file(GLOB FFMPEG_DLLS "${FFMPEG_ROOT}/bin/*.dll")
    file(GLOB SDL3_DLLS "${SDL3_ROOT}/bin/*.dll")

    # Explicit copies are more reliable than runtime dependency discovery on MSYS2 CI.
    set(WINDOWS_RUNTIME_DLLS
        "${binPath}/libgcc_s_seh-1.dll"
        "${binPath}/libiconv-2.dll"
        "${binPath}/libstdc++-6.dll"
        "${binPath}/libwinpthread-1.dll"
        "${binPath}/zlib1.dll"
    )

    set(WINDOWS_PACKAGE_DLLS ${FFMPEG_DLLS} ${SDL3_DLLS})
    foreach(runtime_dll IN LISTS WINDOWS_RUNTIME_DLLS)
        if(EXISTS "${runtime_dll}")
            list(APPEND WINDOWS_PACKAGE_DLLS "${runtime_dll}")
        endif()
    endforeach()

    if(WINDOWS_PACKAGE_DLLS)
        install(FILES
            ${WINDOWS_PACKAGE_DLLS}
            DESTINATION bin
        )
    endif()

    install(FILES
        "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:${CMAKE_CURRENT_BINARY_DIR}/SF3.pdb>"
        DESTINATION bin
    )

    install(FILES
        ${PROJECT_LICENSE_FILES}
        DESTINATION bin/licenses
    )
elseif(UNIX)
    install(TARGETS SF3
        RUNTIME DESTINATION bin
    )

    file(GLOB FFMPEG_SO "${FFMPEG_ROOT}/lib/*.so*")

    install(FILES
        ${FFMPEG_SO}
        ${SDL3_ROOT}/lib/libSDL3.so
        DESTINATION lib
    )

    install(FILES
        ${PROJECT_LICENSE_FILES}
        DESTINATION share/SF3/licenses
    )
endif()

# ======================================
# Packaging
# ======================================

set(CPACK_PACKAGE_NAME "3SX")
set(CPACK_PACKAGE_VENDOR "CrowdedStreet")
# set(CPACK_PACKAGE_VERSION "1.0.0")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else(UNIX)
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
