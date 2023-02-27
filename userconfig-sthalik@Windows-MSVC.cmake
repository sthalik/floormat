set(CMAKE_C_COMPILER_INIT cl.exe)
set(CMAKE_CXX_COMPILER_INIT cl.exe)
set(CMAKE_ASM_NASM_COMPILER nasm.exe)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/msvc.cmake" CACHE FILEPATH "" FORCE)
set(OpenCV_DIR "F:/dev/opentrack-depends/opencv/build-amd64/install" CACHE PATH "" FORCE)

list(APPEND CMAKE_IGNORE_PATH "c:/msys64")
list(APPEND CMAKE_IGNORE_PREFIX_PATH "c:/msys64")
set(CMAKE_INSTALL_MESSAGE NEVER)

function(fm-userconfig-external)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        sets(BOOL
             SDL_STATIC                                         ON
             SDL_SHARED                                         OFF
             CORRADE_BUILD_STATIC                               ON
             CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT    ON
             MAGNUM_BUILD_STATIC                                ON
             MAGNUM_BUILD_PLUGINS_STATIC                        ON
        )
    endif()
endfunction()

sets(BOOL FLOORMAT_PRECOMPILED-HEADERS OFF)

function(fm-userconfig-src)
    add_compile_options(-W4 -Qvec-report:1)
    if(MSVC_VERSION GREATER_EQUAL 1935) # 17.5 Preview
        add_compile_options(-Zc:checkGwOdr)
    endif()
endfunction()
