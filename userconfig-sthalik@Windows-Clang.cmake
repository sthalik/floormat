if(CMAKE_BUILD_TYPE STREQUAL "DEBUG" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(FLOORMAT_COVERAGE)
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
        set(BUILD_SHARED_LIBS OFF)
    else()
        add_link_options(-Wl,--gc-sections)
    endif()
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-Wl,--as-needed)
    add_definitions(-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE)
else()
    set(BUILD_SHARED_LIBS OFF)
    add_compile_options(-emit-llvm)
    add_compile_options(-fmerge-all-constants -flto=full -fwhole-program-vtables -fforce-emit-vtables)
    add_link_options(-fmerge-all-constants -flto=full -fwhole-program-vtables -fforce-emit-vtables)
    add_link_options(-Wl,--lto-O3)
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-Wl,--gc-sections -Wl,--as-needed -Wl,--icf=all)
    add_compile_options(-Wno-nan-infinity-disabled)
    add_definitions(-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)
    add_compile_options(-fpointer-tbaa)
    # Line discriminators for AutoFDO
    add_compile_options(-fdebug-info-for-profiling)
    if(FLOORMAT_PGO STREQUAL "generate" OR FLOORMAT_PGO STREQUAL "cs" OR FLOORMAT_PGO STREQUAL "use") # instrumented PGO, not AutoFDO
        if(NOT FLOORMAT_PGO_PROFDATA)
            set(FLOORMAT_PGO_PROFDATA "${CMAKE_CURRENT_LIST_DIR}/build/pgo.profdata" CACHE FILEPATH "" FORCE)
        endif()
        add_compile_options(-Wno-error=profile-instr-out-of-date -Wno-error=profile-instr-unprofiled
                            -Wno-error=backend-plugin)
        # Only cs and use read the file. A generate tree writes counters and ignores it,
        # so following it there rebuilds every object per re-merge for identical codegen.
        if(NOT FLOORMAT_PGO STREQUAL "generate" AND EXISTS "${FLOORMAT_PGO_PROFDATA}")
            # Unread by any source. The compile line is what makes ninja rebuild, and the
            # profile path does not change when run-pgo.sh re-merges in place.
            file(SHA256 "${FLOORMAT_PGO_PROFDATA}" FLOORMAT_PGO_PROFDATA_SHA)
            string(SUBSTRING "${FLOORMAT_PGO_PROFDATA_SHA}" 0 16 FLOORMAT_PGO_PROFDATA_SHA)
            add_compile_options(-DFM_PGO_PROFILE_HASH=0x${FLOORMAT_PGO_PROFDATA_SHA})
        endif()
    endif()
    # Builds profile-cold functions for size: optsize, minsize, optnone, or default for none.
    # The full-LTO post-link pipeline does not contain it, so these must stay on the compile line.
    # Empty builds cold functions with the normal -O3 pipeline. minsize measured 5.81% smaller
    # and 2.73% slower. The MinGW lld link strips the $unlikely suffix off COMDAT sections, so
    # clang's hot/cold split is lost. Ordering is still available: with a profile the objects
    # carry .llvm.call-graph-profile and lld sorts .text by it, and an explicit order file goes
    # through -Wl,--Xlink=-order:@file.
    if(NOT DEFINED FLOORMAT_PGO_COLD)
        set(FLOORMAT_PGO_COLD "" CACHE STRING "")
    endif()
    if(NOT "${FLOORMAT_PGO_COLD}" STREQUAL "")
        add_compile_options("SHELL:-mllvm -pgo-cold-func-opt=${FLOORMAT_PGO_COLD}")
    endif()
    # Integer, LLVM default 45. Applied as a minimum, so anything above 45 does nothing.
    if(NOT "${FLOORMAT_PGO_COLD_INLINE}" STREQUAL "")
        add_compile_options("SHELL:-mllvm -inline-cold-callsite-threshold=${FLOORMAT_PGO_COLD_INLINE}")
    endif()
    if(FLOORMAT_PGO STREQUAL "generate") # IR instrumentation
        add_compile_options(-fprofile-generate)
        add_link_options(-fprofile-generate)
    elseif(FLOORMAT_PGO STREQUAL "cs") # IR instrumentation again, after inlining
        if(EXISTS "${FLOORMAT_PGO_PROFDATA}")
            add_compile_options(-fprofile-use=${FLOORMAT_PGO_PROFDATA} -fcs-profile-generate)
            add_link_options(-fprofile-use=${FLOORMAT_PGO_PROFDATA} -fcs-profile-generate)
            add_link_options(-Wl,--whole-archive,-lclang_rt.profile-x86_64,--no-whole-archive)
        else()
            message(FATAL_ERROR "FLOORMAT_PGO=cs needs a profile at '${FLOORMAT_PGO_PROFDATA}'; "
                                "run './run-pgo.sh generate' first")
        endif()
    elseif(FLOORMAT_PGO STREQUAL "use") # consume an instrumented profile
        if(EXISTS "${FLOORMAT_PGO_PROFDATA}")
            add_compile_options(-fprofile-use=${FLOORMAT_PGO_PROFDATA})
            add_link_options(-fprofile-use=${FLOORMAT_PGO_PROFDATA})
        else()
            message(STATUS "FLOORMAT_PGO=use: no profile at '${FLOORMAT_PGO_PROFDATA}'; "
                           "train in build/clang-pgo-gen first, then re-run cmake")
        endif()
    elseif(FLOORMAT_PGO STREQUAL "sample") # AutoFDO, sampled from an uninstrumented binary
        if(NOT FLOORMAT_PGO_SAMPLE)
            set(FLOORMAT_PGO_SAMPLE "${CMAKE_CURRENT_LIST_DIR}/build/sample.prof" CACHE FILEPATH "" FORCE)
        endif()
        if(EXISTS "${FLOORMAT_PGO_SAMPLE}")
            add_compile_options(-Wno-error=backend-plugin)
            file(SHA256 "${FLOORMAT_PGO_SAMPLE}" FLOORMAT_PGO_SAMPLE_SHA)
            string(SUBSTRING "${FLOORMAT_PGO_SAMPLE_SHA}" 0 16 FLOORMAT_PGO_SAMPLE_SHA)
            add_compile_options(-DFM_PGO_PROFILE_HASH=0x${FLOORMAT_PGO_SAMPLE_SHA})
            add_compile_options(-fprofile-sample-use=${FLOORMAT_PGO_SAMPLE})
            message(STATUS "FLOORMAT_PGO=sample: using '${FLOORMAT_PGO_SAMPLE}'")
        else()
            message(STATUS "FLOORMAT_PGO=sample: no profile at '${FLOORMAT_PGO_SAMPLE}'; "
                           "trace RELEASE/bin with contrib/xperf-trace.sh, then re-run cmake")
        endif()
    elseif(NOT "${FLOORMAT_PGO}" STREQUAL "")
        message(FATAL_ERROR "FLOORMAT_PGO must be 'generate', 'cs', 'use', 'sample' or empty, "
                            "got '${FLOORMAT_PGO}'")
    endif()
endif()

# TODO use clang-query to find all global and static function-local variables -sh 20250814
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# match varDecl(isStaticLocal())
# match varDecl(hasGlobalStorage(), isStaticDataMember())

if(FLOORMAT_ASAN)
    add_compile_options(-fsanitize=undefined,bounds,address)
    add_link_options(-fsanitize=undefined,bounds,address)
endif()

if(FLOORMAT_ASAN AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/build/external/opencv/clang-asan/.")
    sets(PATH OpenCV_DIR "${CMAKE_CURRENT_LIST_DIR}/build/external/opencv/clang-asan")
elseif(CMAKE_BUILD_TYPE STREQUAL "DEBUG" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/build/external/opencv/clang-debug/.")
    sets(PATH OpenCV_DIR "${CMAKE_CURRENT_LIST_DIR}/build/external/opencv/clang-debug")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/build/external/opencv/clang-release/.")
    sets(PATH OpenCV_DIR "${CMAKE_CURRENT_LIST_DIR}/build/external/opencv/clang-release")
endif()
set(OpenCV_STATIC ON CACHE BOOL "" FORCE)

set(CMAKE_INSTALL_MESSAGE NEVER)

# -mavx for the VEX encoding: bitmask.cpp's SSSE3 intrinsics lose the two-operand
# movdqa copies and run 12% faster (20 paired passes).
# The width pin is a separate decision and is not implied by -mno-avx2: without it
# AVX1 still vectorizes float work to 256 bits, costing 1.2% of .text for no
# measurable time. Integer work is capped at 128 either way.
# sse42, avx1 and avx2 exist to measure what each half costs. Nothing ships them.
if(NOT DEFINED FLOORMAT_SIMD)
    set(FLOORMAT_SIMD "avx128" CACHE STRING "")
endif()
if(FLOORMAT_SIMD STREQUAL "avx128")
    set(fm_simd "-march=x86-64-v2 -mavx -mno-avx2 -mprefer-vector-width=128")
elseif(FLOORMAT_SIMD STREQUAL "avx1")
    set(fm_simd "-march=x86-64-v2 -mavx -mno-avx2")
elseif(FLOORMAT_SIMD STREQUAL "avx2")
    set(fm_simd "-march=x86-64-v2 -mavx2 -maes")
elseif(FLOORMAT_SIMD STREQUAL "sse42")
    set(fm_simd "-march=x86-64-v2")
else()
    message(FATAL_ERROR "FLOORMAT_SIMD must be 'sse42', 'avx1', 'avx128' or 'avx2', "
                        "got '${FLOORMAT_SIMD}'")
endif()
sets(STRING
     CMAKE_C_FLAGS "${fm_simd} -ggdb -gcolumn-info"
     CMAKE_C_FLAGS_DEBUG "-O0 -fstack-protector-all -ggdb -gdwarf-aranges"
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
             FLOORMAT_SUBMODULE-SDL2                            ON
             SDL_SHARED                                         OFF
             SDL_STATIC                                         ON
             CORRADE_BUILD_STATIC                               ON
             CORRADE_BUILD_TESTS                                ON
             CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT    ON
             MAGNUM_BUILD_PLUGINS_STATIC                        ON
             MAGNUM_BUILD_STATIC                                ON
             MAGNUM_BUILD_TESTS                                 ON
        )
    else()
        sets(BOOL
             FLOORMAT_SUBMODULE-SDL2                            ON
             SDL_SHARED                                         ON
             SDL_STATIC                                         OFF
             CORRADE_BUILD_STATIC                               OFF
             CORRADE_BUILD_TESTS                                ON
             CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT    OFF
             MAGNUM_BUILD_PLUGINS_STATIC                        OFF
             MAGNUM_BUILD_STATIC                                OFF
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
        -Wno-lifetime-safety-cross-tu-suggestions
        -Wno-lifetime-safety-intra-tu-suggestions
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
        -Wno-error=nrvo
        -Wno-error=missing-noreturn
        -Wno-error=lifetime-safety-cross-tu-suggestions
        -Wno-error=lifetime-safety-intra-tu-suggestions
    )
    add_compile_options(
        #-Wglobal-constructors
        -Wno-global-constructors
        #-Wexit-time-destructors
        -Wno-exit-time-destructors
    )
    if(CMAKE_BUILD_TYPE STREQUAL "DEBUG" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-ftime-trace)
    endif()
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:SHELL:-Xclang -flifetime-safety-inference>")
    #add_link_options(-Wl,-mllvm,-pass-remarks=wholeprogramdevirt)
    add_compile_definitions("$<$<NOT:$<CONFIG:Debug,DEBUG>>:-DFM_NO_DEBUG3>")

    if(FLOORMAT_COVERAGE)
        sets(BOOL
             FLOORMAT_SUBMODULE-SDL2                            ON
             SDL_SHARED                                         OFF
             SDL_STATIC                                         ON
             CORRADE_BUILD_STATIC                               ON
             MAGNUM_BUILD_PLUGINS_STATIC                        ON
             MAGNUM_BUILD_STATIC                                ON
        )
    endif()
endfunction()
