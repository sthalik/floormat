sets(BOOL FLOORMAT_SUBMODULE-SDL2 OFF)
set(CMAKE_INSTALL_MESSAGE NEVER)
sets(STRING
     CMAKE_BUILD_TYPE RELEASE
     CMAKE_C_FLAGS_RELEASE "-O0 -DNDEBUG -g -ggdb"
     CMAKE_CXX_FLAGS_RELEASE "-O0 -DNDEBUG -g -ggdb")

add_compile_options(-fsanitize=address,undefined)
add_link_options(-fsanitize=address,undefined)
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
endfunction()
