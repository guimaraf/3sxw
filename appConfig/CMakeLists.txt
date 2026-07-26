file(GLOB CONFIG_APP_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

if(APPLE)
    add_executable(SF3Config MACOSX_BUNDLE ${CONFIG_APP_SRC})
elseif(WIN32)
    add_executable(SF3Config WIN32 ${CONFIG_APP_SRC})
else()
    add_executable(SF3Config ${CONFIG_APP_SRC})
endif()

if(WIN32)
    set(SF3CONFIG_ICON_FILE "${CMAKE_SOURCE_DIR}/img/Hugo.ico")

    if(NOT EXISTS "${SF3CONFIG_ICON_FILE}")
        message(FATAL_ERROR "Required configurator icon was not found: ${SF3CONFIG_ICON_FILE}")
    endif()

    file(TO_CMAKE_PATH "${SF3CONFIG_ICON_FILE}" SF3CONFIG_ICON_FILE)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/sf3config.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/sf3config.rc"
        @ONLY
    )
    target_sources(SF3Config PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/sf3config.rc")
endif()

target_include_directories(SF3Config PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${SDL3_ROOT}/include"
)

target_compile_features(SF3Config PRIVATE c_std_11)
target_compile_options(SF3Config PRIVATE -Wall -Werror)

set_target_properties(SF3Config PROPERTIES
    OUTPUT_NAME "sf3config"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

foreach(configuration Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${configuration}" configuration_upper)
    set_target_properties(SF3Config PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${configuration_upper}" "${CMAKE_CURRENT_BINARY_DIR}"
    )
endforeach()

if(APPLE)
    target_link_libraries(SF3Config PRIVATE "${SDL3_ROOT}/lib/libSDL3.0.dylib")
    set_target_properties(SF3Config PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "3SX Configurator"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )

    install(TARGETS SF3Config
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
    )

    install(FILES "${SDL3_ROOT}/lib/libSDL3.0.dylib"
        DESTINATION sf3config.app/Contents/Frameworks
    )
elseif(WIN32)
    target_link_libraries(SF3Config PRIVATE
        "${SDL3_ROOT}/lib/libSDL3.dll.a"
        gdi32
    )
    install(TARGETS SF3Config RUNTIME DESTINATION bin)
else()
    target_link_libraries(SF3Config PRIVATE "${SDL3_ROOT}/lib/libSDL3.so")
    set_target_properties(SF3Config PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")
    install(TARGETS SF3Config RUNTIME DESTINATION bin)
endif()
