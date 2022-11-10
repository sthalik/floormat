sets(BOOL FLOORMAT_PRECOMPILED-HEADERS OFF)

# for floormat sources only
function(fm-userconfig-src)
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wno-old-style-cast -Wno-padded
    )
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        #-Weverything
        -Wno-c++20-compat
        -Wno-switch-enum
        #-Wno-old-style-cast
        #-Wno-shadow
        -Wno-ctad-maybe-unsupported
        -Wno-ignored-attributes
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
