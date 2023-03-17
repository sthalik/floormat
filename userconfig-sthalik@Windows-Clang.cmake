if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    add_definitions(-D_LIBCPP_ENABLE_ASSERTIONS=1)
    add_compile_options(-mavx2)
    if(FLOORMAT_WITH-COVERAGE)
        set(CMAKE_BUILD_TYPE DEBUG CACHE STRING "" FORCE)
        add_definitions(
            -fprofile-instr-generate
            -fcoverage-mapping
            -mllvm -runtime-counter-relocation=true
        )
        add_link_options(
            -fprofile-instr-generate
            -fcoverage-mapping
        )
    endif()
    sets(BOOL FLOORMAT_PRECOMPILED-HEADERS OFF)
else()
    add_compile_options(-march=native -mavx2)
    add_compile_options(-emit-llvm)
    add_compile_options(-fmerge-all-constants -flto=full -fwhole-program-vtables -fforce-emit-vtables)
    add_link_options(-fmerge-all-constants -flto=full -fwhole-program-vtables -fforce-emit-vtables)
    add_link_options(-Wl,--gc-sections -Wl,--icf=all)
    sets(BOOL FLOORMAT_PRECOMPILED-HEADERS ON)
endif()
set(CMAKE_INSTALL_MESSAGE NEVER)

if(FLOORMAT_ASAN)
    add_compile_options(-fsanitize=undefined -fsanitize=address)
    add_link_options(-fsanitize=undefined -fsanitize=address)
endif()

set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../opentrack-depends/opencv/build-clang-amd64/install" CACHE PATH "" FORCE)

sets(STRING
     CMAKE_C_FLAGS ""
     CMAKE_C_FLAGS_DEBUG "-O0 -g -ggdb -gcolumn-info -gmodules -gdwarf-aranges -gz=zlib -fstack-protector-all"
     CMAKE_C_FLAGS_RELEASE "-O3 -ffast-math -ftree-vectorize -march=core2 -mtune=skylake -mtune=native -mavx"
     CMAKE_EXE_LINKER_FLAGS_DEBUG ""
     CMAKE_SHARED_LINKER_FLAGS_DEBUG ""
)

sets(STRING
     CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}"
     CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}"
     CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}"
)

add_compile_options(-Xclang -fcolor-diagnostics -fdiagnostics-color=always)

# for building submodule dependencies
function(fm-userconfig-external)
    add_compile_options(
        -Wno-ignored-attributes
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-error=return-type
    )
    sets(BOOL
         CORRADE_BUILD_TESTS                                    ON
         MAGNUM_BUILD_TESTS                                     ON
    )
    if(NOT CMAKE_BUILD_TYPE STREQUAL "DEBUG" OR FLOORMAT_ASAN)
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

# for test_app sources only
function(fm-userconfig-src)
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wno-old-style-cast -Wno-padded -Weverything
    )
    add_compile_options(
        -Wno-c++98-compat
        -Wno-c++20-compat
        -Wno-c++98-compat-pedantic
        -Wno-logical-op-parentheses
        -Wno-undefined-func-template
        -Wno-switch-enum
        -Wno-covered-switch-default
        -Wno-old-style-cast
        -Wno-global-constructors
        -Wno-exit-time-destructors
        -Wno-implicit-int-float-conversion
        -Wno-shadow-field-in-constructor
        -Wno-shadow-field
        -Wno-shadow
        -Wno-ctad-maybe-unsupported
        -Wno-documentation-unknown-command
        -Wno-documentation
        -Wno-ignored-attributes
        -Wno-reserved-identifier
        -Wno-zero-length-array
        -Wno-comma
    )
    add_compile_options(
        -Werror
        -Wno-error=float-equal          
        #-Wno-error=comma
        -Wno-error=unused-parameter
        -Wno-error=unused-private-field
        -Wno-error=unused-variable
        -Wno-error=unused-function
        -Wno-error=unused-member-function
        -Wno-error=unused-macros
        -Wno-error=alloca
        -Wno-error=double-promotion
        -Wno-error=ambiguous-reversed-operator
    )
endfunction()
