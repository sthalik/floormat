set(CMAKE_C_COMPILER_INIT cl.exe)
set(CMAKE_CXX_COMPILER_INIT cl.exe)
set(CMAKE_ASM_NASM_COMPILER nasm.exe)

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING "" FORCE)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/msvc.cmake" CACHE FILEPATH "" FORCE)

if(NOT DEFINED OpenCV_DIR)
    if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        #set(OpenCV_DIR "F:/dev/opentrack-depends/opencv/build-msvc64-debug/install" CACHE PATH "" FORCE)
        sets(PATH OpenCV_DIR "")
    else()
        set(OpenCV_DIR "F:/dev/opentrack-depends/opencv/build-amd64/install" CACHE PATH "" FORCE)
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(BUILD_SHARED_LIBS OFF)
endif()

list(APPEND CMAKE_IGNORE_PREFIX_PATH "c:/msys64")
set(CMAKE_INSTALL_MESSAGE NEVER)
sets(PATH CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")

sets(BOOL FLOORMAT_SUBMODULE-SDL2 ON)

add_link_options(-STACK:41943040)
add_link_options(-WX:NO)

if(FLOORMAT_ASAN)
    add_compile_options(-fsanitize-address-use-after-return -fsanitize=address)
endif()

if(CMAKE_SIZEOF_VOID_P GREATER 4)
    set(CMAKE_C_COMPILER_LAUNCHER msvc64.cmd)
    set(CMAKE_CXX_COMPILER_LAUNCHER msvc64.cmd)
    set(CMAKE_C_LINKER_LAUNCHER msvc64.cmd)
    set(CMAKE_CXX_LINKER_LAUNCHER msvc64.cmd)
    set(CMAKE_RC_COMPILER_LAUNCHER msvc64.cmd)
    SET(CMAKE_RC_LINKER_LAUNCHER msvc64.cmd)
else()
    set(CMAKE_C_COMPILER_LAUNCHER msvc.cmd)
    set(CMAKE_CXX_COMPILER_LAUNCHER msvc.cmd)
    set(CMAKE_C_LINKER_LAUNCHER msvc.cmd)
    set(CMAKE_CXX_LINKER_LAUNCHER msvc.cmd)
    set(CMAKE_RC_COMPILER_LAUNCHER msvc.cmd)
    SET(CMAKE_RC_LINKER_LAUNCHER msvc.cmd)
endif()

function(fm-userconfig-external)
    if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        sets(BOOL
             CORRADE_BUILD_TESTS                                ON
             MAGNUM_BUILD_TESTS                                 ON
             SDL_STATIC                                         OFF
             SDL_SHARED                                         ON
             SDL_FORCE_STATIC_VCRT                              OFF
             SDL_LIBC                                           ON
             CORRADE_BUILD_STATIC                               OFF
             CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT    OFF
             MAGNUM_BUILD_STATIC                                OFF
             MAGNUM_BUILD_PLUGINS_STATIC                        OFF
        )
    else()
        add_definitions(-DCORRADE_TARGET_AVX_FMA)
        sets(BOOL
             SDL_STATIC                                         ON
             SDL_SHARED                                         OFF
             SDL_FORCE_STATIC_VCRT                              ON
             CORRADE_BUILD_TESTS                                OFF
             MAGNUM_BUILD_TESTS                                 OFF
             CORRADE_BUILD_STATIC                               ON
             CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT    ON
             MAGNUM_BUILD_STATIC                                ON
             MAGNUM_BUILD_PLUGINS_STATIC                        ON
        )
    endif()
endfunction()

function(fm-userconfig-src)
    add_compile_options(-W4 -Qvec-report:1)
    add_compile_options(
        -wd4702 # warning C4702: unreachable code
    )
    if(MSVC_VERSION GREATER_EQUAL 1935) # 17.5 Preview
        add_compile_options(-Zc:checkGwOdr)
    endif()
endfunction()
