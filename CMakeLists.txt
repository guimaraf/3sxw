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

    $<$<CONFIG:Release>:RELEASE>
    $<$<CONFIG:Release>:CHECKSUM>

    $<$<PLATFORM_ID:Darwin>:_DARWIN_C_SOURCE>
    $<$<NOT:$<PLATFORM_ID:Darwin>>:_POSIX_C_SOURCE=200809L>

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
set(MINIZIP_NG_ROOT    "${THIRD_PARTY_DIR}/minizip-ng/build")
set(TF_PSA_CRYPTO_ROOT "${THIRD_PARTY_DIR}/tf-psa-crypto/build")

include_directories(
    ${FFMPEG_ROOT}/include
    ${SDL3_ROOT}/include
    ${LIBCDIO_ROOT}/include
    ${MINIZIP_NG_ROOT}/include
    ${TF_PSA_CRYPTO_ROOT}/include
)

target_link_libraries(SF3 PRIVATE
    m
    "${LIBCDIO_ROOT}/lib/libiso9660.a"
    "${LIBCDIO_ROOT}/lib/libcdio.a"
    "${MINIZIP_NG_ROOT}/lib/libminizip-ng.a"
    "${TF_PSA_CRYPTO_ROOT}/lib/libtfpsacrypto.a"
    ZLIB::ZLIB
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

# Build the standalone configuration application in its requested directory.
add_subdirectory(appConfig "${CMAKE_SOURCE_DIR}/appConfig/build")

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

    # Inspect the actual PE imports, including dependencies of dependencies.
    # Raw .dll.a link paths do not populate TARGET_RUNTIME_DLLS reliably.
    if(NOT CMAKE_OBJDUMP)
        find_program(CMAKE_OBJDUMP NAMES objdump llvm-objdump HINTS "${binPath}" REQUIRED)
    endif()
    if(NOT EXISTS "${CMAKE_OBJDUMP}")
        message(FATAL_ERROR "A PE inspection tool (CMAKE_OBJDUMP) is required to package Windows DLLs.")
    endif()

    set(WINDOWS_DEPENDENCY_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/windows-runtime-dependencies.cmake")
    file(CONFIGURE OUTPUT "${WINDOWS_DEPENDENCY_SCRIPT}" CONTENT [=[
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "@CMAKE_OBJDUMP@")

if(NOT DEFINED runtime_executable OR NOT EXISTS "${runtime_executable}")
    message(FATAL_ERROR "Windows executable does not exist: ${runtime_executable}")
endif()

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${runtime_executable}"
    DIRECTORIES "@FFMPEG_ROOT@/bin" "@SDL3_ROOT@/bin" "@binPath@"
    RESOLVED_DEPENDENCIES_VAR resolved_dlls
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dlls
    CONFLICTING_DEPENDENCIES_PREFIX conflicting_dlls
    PRE_EXCLUDE_REGEXES "^[Aa][Pp][Ii]-[Mm][Ss]-" "^[Ee][Xx][Tt]-[Mm][Ss]-"
    POST_EXCLUDE_REGEXES ".*[/\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\].*"
)

if(unresolved_dlls OR conflicting_dlls_FILENAMES)
    message(FATAL_ERROR
        "Cannot package ${runtime_executable}. Missing DLLs: ${unresolved_dlls}; conflicting DLLs: ${conflicting_dlls_FILENAMES}")
endif()

foreach(runtime_dll IN LISTS resolved_dlls)
    message(STATUS "Windows runtime dependency: ${runtime_dll}")
    if(DEFINED runtime_destination)
        file(MAKE_DIRECTORY "${runtime_destination}")
        get_filename_component(runtime_source_dir "${runtime_dll}" DIRECTORY)
        file(REAL_PATH "${runtime_source_dir}" runtime_source_dir)
        file(REAL_PATH "${runtime_destination}" runtime_destination_dir)
        if(NOT runtime_source_dir STREQUAL runtime_destination_dir)
            get_filename_component(runtime_filename "${runtime_dll}" NAME)
            file(COPY_FILE "${runtime_dll}" "${runtime_destination}/${runtime_filename}" ONLY_IF_DIFFERENT)
        endif()
    endif()
endforeach()
]=] @ONLY)

    foreach(runtime_target IN ITEMS SF3 SF3Config)
        add_custom_target(${runtime_target}RuntimeDLLs ALL
            COMMAND "${CMAKE_COMMAND}"
                "-Druntime_executable=$<TARGET_FILE:${runtime_target}>"
                "-Druntime_destination=$<TARGET_FILE_DIR:${runtime_target}>"
                -P "${WINDOWS_DEPENDENCY_SCRIPT}"
            COMMENT "Copying Windows runtime DLLs for ${runtime_target}"
            DEPENDS ${runtime_target}
            VERBATIM
        )
        install(CODE "
            execute_process(
                COMMAND \"${CMAKE_COMMAND}\"
                    \"-Druntime_executable=$<TARGET_FILE:${runtime_target}>\"
                    \"-Druntime_destination=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin\"
                    -P \"${WINDOWS_DEPENDENCY_SCRIPT}\"
                COMMAND_ERROR_IS_FATAL ANY
            )
        ")
    endforeach()

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

set(BEZEL_SOURCE_DIR "${CMAKE_SOURCE_DIR}/img/16-9")
set(BEZEL_IMAGE_FILE "${BEZEL_SOURCE_DIR}/bezel.png")
set(PIXEL_PERFECT_BEZEL_IMAGE_FILE "${BEZEL_SOURCE_DIR}/bezel-pixel-perfect.png")
set(BEZEL_2_IMAGE_FILE "${BEZEL_SOURCE_DIR}/bezel2.png")
set(BEZEL_3_IMAGE_FILE "${BEZEL_SOURCE_DIR}/bezel3.png")

foreach(REQUIRED_BEZEL_IMAGE_FILE IN ITEMS
        "${BEZEL_IMAGE_FILE}"
        "${PIXEL_PERFECT_BEZEL_IMAGE_FILE}"
        "${BEZEL_2_IMAGE_FILE}"
        "${BEZEL_3_IMAGE_FILE}")
    if(NOT EXISTS "${REQUIRED_BEZEL_IMAGE_FILE}")
        message(FATAL_ERROR "Required bezel image was not found: ${REQUIRED_BEZEL_IMAGE_FILE}")
    endif()
endforeach()

if(APPLE)
    set(GAME_DATA_INSTALL_DIR "3SX.app/Contents/MacOS/data")
else()
    set(GAME_DATA_INSTALL_DIR "bin/data")
endif()

install(FILES
    "${BEZEL_IMAGE_FILE}"
    "${PIXEL_PERFECT_BEZEL_IMAGE_FILE}"
    "${BEZEL_2_IMAGE_FILE}"
    "${BEZEL_3_IMAGE_FILE}"
    DESTINATION "${GAME_DATA_INSTALL_DIR}/img"
)

# ======================================
# Packaging
# ======================================

set(CPACK_PACKAGE_NAME "3SX")
set(CPACK_PACKAGE_VENDOR "CrowdedStreet")
set(CPACK_PACKAGE_VERSION "1.1.1")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else(UNIX)
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
