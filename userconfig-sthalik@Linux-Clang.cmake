set(fm_simd "-march=x86-64-v2 -mavx -mno-avx2 -mprefer-vector-width=128")

sets(STRING
     CMAKE_C_FLAGS "${fm_simd} -ggdb -gcolumn-info"
     CMAKE_C_FLAGS_DEBUG "-O0 -g -fstack-protector-all"
     CMAKE_C_FLAGS_RELEASE "-O3 -mpopcnt -fomit-frame-pointer -fno-stack-protector"
)

sets(STRING
     CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}"
     CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}"
     CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}"
)

add_link_options(-fuse-ld=lld)

if(NOT CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(BUILD_SHARED_LIBS OFF)
    add_compile_options(-fmerge-all-constants -flto=full -fwhole-program-vtables -fforce-emit-vtables)
    add_link_options(-fmerge-all-constants -flto=full -fwhole-program-vtables -fforce-emit-vtables)
    add_link_options(-Wl,--lto-O3)
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-Wl,--gc-sections -Wl,--as-needed -Wl,--icf=all)
    add_compile_options(-Wno-nan-infinity-disabled)
    add_compile_options(-fpointer-tbaa)
    add_compile_options(
        -ffp-model=aggressive -ffp-contract=fast
        -fno-honor-infinities -fno-honor-nans
        -fno-math-errno -fno-signed-zeros -fno-trapping-math -fno-rounding-math
        -freciprocal-math -fassociative-math -fapprox-func
        -fcomplex-arithmetic=basic -Wno-overriding-complex-range
    )
    add_link_options(-mdaz-ftz)
    add_compile_options(
        -mtune=generic
        -mno-vzeroupper
        -fstrict-vtable-pointers
        -fstrict-enums
        -fnew-infallible
    )
    add_link_options(
        -Wl,-mllvm,-enable-ext-tsp-block-placement
        -Wl,-mllvm,-hot-cold-split
    )
    # burzum's i5-7600 runs JCC-erratum microcode: a branch crossing or ending on a 32-byte
    # boundary isn't kept in the uop cache, so code layout alone moved A/B timings up to 17 %.
    add_compile_options(-mbranches-within-32B-boundaries)
    add_link_options(-mbranches-within-32B-boundaries)
    # Skylake JCC erratum: also pads calls and returns.
    #add_compile_options(-malign-branch=fused,jcc,jmp,call,ret,indirect)
    #add_link_options(-malign-branch=fused,jcc,jmp,call,ret,indirect)
endif()

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fassume-nothrow-exception-dtor>)
set(CMAKE_INSTALL_MESSAGE NEVER)
set(FLOORMAT_SUBMODULE-SDL2 1)

function(fm-userconfig-external)
    add_compile_options(
        -Wno-ignored-attributes
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-error=return-type
    )
    sets(BOOL
         SDL_SHARED                                         OFF
         SDL_STATIC                                         ON
         CORRADE_BUILD_STATIC                               ON
         CORRADE_BUILD_TESTS                                OFF
         CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT    ON
         MAGNUM_BUILD_PLUGINS_STATIC                        ON
         MAGNUM_BUILD_STATIC                                ON
         MAGNUM_BUILD_TESTS                                 OFF
    )
endfunction()

function(fm-userconfig-src)
    add_compile_options(-Wall -Wextra)
    add_compile_definitions("$<$<NOT:$<CONFIG:Debug,DEBUG>>:FM_NO_DEBUG3>")
endfunction()
