if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
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
    add_definitions(-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE)
else()
    set(BUILD_SHARED_LIBS OFF)
    add_compile_options(-march=native -mavx2)
    add_compile_options(-emit-llvm)
    add_compile_options(-fmerge-all-constants -flto=full -fwhole-program-vtables -fforce-emit-vtables)
    add_link_options(-fmerge-all-constants -flto=full -fwhole-program-vtables -fforce-emit-vtables)
    add_link_options(-Wl,--gc-sections -Wl,--icf=all)
    add_compile_options(-Wno-nan-infinity-disabled)
    add_definitions(-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)
endif()

if(FLOORMAT_ASAN)
    add_compile_options(-fsanitize=undefined,bounds,address)
    add_link_options(-fsanitize=undefined,bounds,address)
endif()

set(OpenCV_DIR "f:/build/opencv/build-clang-release-floormat/install" CACHE PATH "" FORCE)
set(CMAKE_INSTALL_MESSAGE NEVER)

sets(STRING
     CMAKE_C_FLAGS "-march=x86-64-v2 -mtune=native -mavx2 -maes -g -gcolumn-info -gdwarf-aranges"
     CMAKE_C_FLAGS_DEBUG "-O0 -fstack-protector-all -g"
     CMAKE_C_FLAGS_RELEASE "-O3 -ffast-math -mpopcnt -fomit-frame-pointer -fno-stack-protector -static"
     CMAKE_EXE_LINKER_FLAGS_DEBUG ""
     CMAKE_SHARED_LINKER_FLAGS_DEBUG ""
)
sets(STRING
     CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}"
     CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}"
     CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}"
)

add_definitions(-D_LIBCPP_REMOVE_TRANSITIVE_INCLUDES)

if(NOT CMAKE_CXX_COMPILER_VERSION LESS "18.0")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fassume-nothrow-exception-dtor>)
endif()

# for building submodule dependencies
function(fm-userconfig-external)
    add_compile_options(
        -Wno-ignored-attributes
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-error=return-type
    )
    if(NOT CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        sets(BOOL
             #FLOORMAT_SUBMODULE-SDL2                           ON
             SDL_SHARED                                         OFF
             SDL_STATIC                                         ON
             CORRADE_BUILD_STATIC                               ON
             CORRADE_BUILD_TESTS                                OFF
             CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT    ON
             MAGNUM_BUILD_PLUGINS_STATIC                        ON
             MAGNUM_BUILD_STATIC                                ON
             MAGNUM_BUILD_TESTS                                 ON
        )
    else()
        sets(BOOL
             FLOORMAT_SUBMODULE-SDL2                            OFF
             SDL_SHARED                                         ON
             SDL_STATIC                                         OFF
             CORRADE_BUILD_STATIC                               OFF
             CORRADE_BUILD_TESTS                                OFF
             CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT    OFF
             MAGNUM_BUILD_PLUGINS_STATIC                        OFF
             MAGNUM_BUILD_STATIC                                OFF
             MAGNUM_BUILD_TESTS                                 ON
        )
    endif()
    if(FLOORMAT_ASAN)
        sets(BOOL
             CORRADE_BUILD_STATIC                               ON
             CORRADE_BUILD_TESTS                                ON
             MAGNUM_BUILD_STATIC                                ON
             MAGNUM_BUILD_TESTS                                 ON
        )
    endif()
endfunction()

# for test_app sources only
function(fm-userconfig-src)
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-old-style-cast -Wno-padded -Weverything)
    add_compile_options(
        -Wno-c++98-compat
        -Wno-c++20-compat
        -Wno-c++98-compat-pedantic
        -Wno-logical-op-parentheses
        -Wno-undefined-func-template
        -Wno-switch-enum
        -Wno-covered-switch-default
        -Wno-old-style-cast
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
        -Wno-unsafe-buffer-usage
        -Wno-bitwise-op-parentheses
        -Wno-weak-vtables
        -Wno-c99-compat
        -Wno-switch-default
        #-Wno-deprecated-literal-operator -Wno-error=deprecated-literal-operator
    )
    add_compile_options(
        -Werror
        -Wno-error=float-equal          
        -Wno-error=unused-parameter
        -Wno-error=unused-private-field
        -Wno-error=unused-variable
        -Wno-error=unused-function
        -Wno-error=unused-template
        -Wno-error=unused-member-function
        -Wno-error=unused-macros
        -Wno-error=alloca
        -Wno-error=double-promotion
        -Wno-error=ambiguous-reversed-operator
        -Wno-error=comma
        -Wno-error=weak-vtables
        -Wno-error=unreachable-code
        -Wno-error=unused-command-line-argument
        -Wno-error=switch-default
        #-Wno-error=switch
        -Wno-error=global-constructors
        -Wno-error=exit-time-destructors
    )
    add_compile_options(
        #-Wglobal-constructors
        -Wno-global-constructors
        #-Wexit-time-destructors
        -Wno-exit-time-destructors
    )
    if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        add_compile_options(-ftime-trace)
    endif()
endfunction()
