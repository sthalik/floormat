sets(STRING
     CMAKE_C_FLAGS ""
     CMAKE_C_FLAGS_DEBUG "-O0 -g -ggdb -fstack-protector-all"
     CMAKE_C_FLAGS_RELEASE "-O3 -ffast-math -ftree-vectorize -march=core2 -mtune=native -mavx -flto -fipa-pta -static"
)

sets(STRING
     CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}"
     CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}"
     CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}"
)

list(APPEND CMAKE_IGNORE_PATH "c:/msys64" "c:/msys64/clang64")
list(APPEND CMAKE_IGNORE_PREFIX_PATH "c:/msys64" "c:/msys64/clang64")

add_compile_options(-fdiagnostics-color=always)
set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../opentrack-depends/opencv/build-gcc/install" CACHE PATH "" FORCE)

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    add_definitions(-D_GLIBCXX_ASSERTIONS)
endif()

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
    endif()

endfunction()

# for floormat sources only
function(fm-userconfig-src)
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wno-old-style-cast -Wno-padded
        -fconcepts-diagnostics-depth=2
    )
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
        #-Werror
        -Wno-error=float-equal
        #-Wno-error=comma
        -Wno-error=unused-parameter
        -Wno-error=unused-variable
        -Wno-error=unused-function
        -Wno-error=unused-macros
        -Wno-error=double-promotion
        -Wno-error=restrict
        -Wno-error=unused-but-set-variable
        -Wno-error=subobject-linkage
        -Wno-error=array-bounds
    )
endfunction()
