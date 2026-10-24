sets(BOOL FLOORMAT_SUBMODULE-SDL2 OFF)
set(CMAKE_INSTALL_MESSAGE NEVER)
# Memcheck can't run ASan binaries. Its manual advises against -O2 and above.
if(FLOORMAT_VALGRIND)
    sets(STRING
         CMAKE_BUILD_TYPE Debug
         CMAKE_C_FLAGS_DEBUG "-O -ggdb"
         CMAKE_CXX_FLAGS_DEBUG "-O -ggdb")
else()
    sets(STRING
         CMAKE_BUILD_TYPE Debug
         CMAKE_C_FLAGS_DEBUG "-O3 -DNDEBUG -g -ggdb"
         CMAKE_CXX_FLAGS_DEBUG "-O3 -DNDEBUG -g -ggdb")
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

add_definitions(-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2)
sets(BOOL CORRADE_CPU_USE_IFUNC OFF)

# for floormat sources only
function(fm-userconfig-src)
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        #-Weverything
        #-Wno-c++20-compat
        -Wno-switch-enum
        #-Wno-old-style-cast
        #-Wno-shadow
        -Wno-ctad-maybe-unsupported
        -Wno-ignored-attributes
        #-Wno-array-bounds
        -Wno-subobject-linkage
        -Wno-old-style-cast
        -Wno-padded
        -Wdangling-reference
        -Wreturn-local-addr
    )
    add_compile_options(
        #-Werror
        -Wno-error=float-equal
        #-Wno-error=comma
        -Wno-error=unused-parameter
        -Wno-error=unused-variable
        -Wno-error=unused-function
        -Wno-error=unused-macros
        #-Wno-error=alloca
        -Wno-error=double-promotion
        -Wno-error=restrict
        -Wno-error=unused-but-set-variable
        -Wno-error=subobject-linkage
        -Wno-error=array-bounds
    )
    # Under C++26, GCC zero-fills uninitialized automatic variables, so memcheck never
    # sees a read of one.
    if(FLOORMAT_VALGRIND)
        add_compile_options(-ftrivial-auto-var-init=uninitialized)
    else()
        # ASan doesn't detect uninitialized reads. With 0xFE fill, a pointer or index
        # read from one faults, and UBSan reports a bool loaded from one. MSan, like
        # memcheck, sees filled bytes as written and can't report the read, so a build
        # with -fsanitize=memory needs =uninitialized.
        add_compile_options(-ftrivial-auto-var-init=pattern)
    endif()
endfunction()

function(fm-userconfig-post)
    target_compile_options(floormat-hwy PRIVATE -O0)
endfunction()
