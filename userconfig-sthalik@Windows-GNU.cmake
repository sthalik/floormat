sets(STRING
     CMAKE_C_FLAGS ""
     CMAKE_C_FLAGS_DEBUG "-O0 -g -ggdb -fstack-protector-all -fstack-reuse=none"
     CMAKE_C_FLAGS_RELEASE "-O3 -ffast-math -ftree-vectorize -funsafe-loop-optimizations -march=core2 -mtune=native -mavx -flto -fipa-pta -fmerge-all-constants -static"
)

sets(STRING
     CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}"
     CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}"
     CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}"
)

list(APPEND CMAKE_IGNORE_PATH "c:/msys64" "c:/msys64/clang64")
list(APPEND CMAKE_IGNORE_PREFIX_PATH "c:/msys64" "c:/msys64/clang64")

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fconcepts-diagnostics-depth=3>)
add_compile_options(-fdiagnostics-color=always)
set(OpenCV_DIR "f:/dev/opentrack-depends/opencv/build-gcc/install" CACHE PATH "" FORCE)
add_compile_options(-fstack-usage -Wstack-usage=16384)

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    add_definitions(-D_GLIBCXX_ASSERTIONS)
    add_definitions(-D_GLIBCXX_USE_DEPRECATED=0 -D_GLIBCXX_USE_CXX11_ABI)
    add_definitions(-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC)
else()
    set(BUILD_SHARED_LIBS OFF)
endif()
add_compile_definitions("$<$<CONFIG:Debug,DEBUG>:_FORTIFY_SOURCE=3>")
add_compile_definitions("$<IF:$<CONFIG:Debug,DEBUG>,,_FORTIFY_SOURCE=3>")

set(FLOORMAT_SUBMODULE-SDL2 1)

# for building submodule dependencies
function(fm-userconfig-external)
    add_compile_options(
        -Wno-ignored-attributes
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-restrict
        -Wno-uninitialized
    )
    if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        sets(BOOL
             CORRADE_BUILD_TESTS                                    ON
             MAGNUM_BUILD_TESTS                                     ON
             SDL_STATIC                                             OFF
             SDL_SHARED                                             ON
             CORRADE_BUILD_STATIC                                   OFF
             CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT        OFF
             MAGNUM_BUILD_STATIC                                    OFF
             MAGNUM_BUILD_PLUGINS_STATIC                            OFF
             MAGNUM_BUILD_STATIC_UNIQUE_GLOBALS                     ON
        )
    else()
        sets(BOOL
             CORRADE_BUILD_TESTS                                    OFF
             MAGNUM_BUILD_TESTS                                     OFF
             SDL_STATIC                                             ON
             SDL_SHARED                                             OFF
             CORRADE_BUILD_STATIC                                   ON
             CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT        ON
             MAGNUM_BUILD_STATIC                                    ON
             MAGNUM_BUILD_PLUGINS_STATIC                            ON
             MAGNUM_BUILD_STATIC_UNIQUE_GLOBALS                     OFF
        )
        set(OpenCV_DIR "OpenCV_DIR-NOTFOUND" CACHE STRING "" FORCE)
    endif()
endfunction()

# for floormat sources only
function(fm-userconfig-src)
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wno-old-style-cast -Wno-padded
        -Wstringop-overflow -Wstringop-truncation
        -Wswitch-enum -Wlarger-than=8192
        -Wlogical-op
        -Wunsafe-loop-optimizations
        #-fconcepts-diagnostics-depth=2
    )
    #add_compile_options(-Wuseless-cast)
    add_link_options(-Wno-lto-type-mismatch -Wno-odr)
    add_compile_options(
        #-Wno-c++20-compat
        -Wno-switch-enum
        -Wno-ctad-maybe-unsupported
        -Wno-ignored-attributes
        -Wno-parentheses
        -Wno-lto-type-mismatch -Wno-odr
    )
    add_compile_options(
        -Werror=format
        -Werror
        -Wno-error=float-equal
        #-Wno-error=comma
        -Wno-error=unused-parameter
        -Wno-error=unused-variable
        -Wno-error=unused-function
        -Wno-error=unused-macros
        #-Wno-error=double-promotion
        -Wdouble-promotion -Werror=double-promotion
        -Wno-error=restrict
        -Wno-error=unused-but-set-variable
        -Wno-error=subobject-linkage
        -Wno-error=array-bounds
        -Wlarger-than=65535 -Wno-error=larger-than=65535
    )
    add_compile_options(
        -Wdelete-incomplete -Werror=delete-incomplete
    )
endfunction()
