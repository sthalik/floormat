sets(BOOL FLOORMAT_SUBMODULE-SDL2 OFF)
set(CMAKE_INSTALL_MESSAGE NEVER)
sets(STRING
     CMAKE_BUILD_TYPE DEBUG
     CMAKE_C_FLAGS_DEBUG "-O3 -DNDEBUG -g -ggdb"
     CMAKE_CXX_FLAGS_DEBUG "-O3 -DNDEBUG -g -ggdb")

add_compile_options(-fsanitize=address,undefined)
add_link_options(-fsanitize=address,undefined)
sets(BOOL CORRADE_CPU_USE_IFUNC OFF)

function(fm-userconfig-external)
    add_compile_options(-Wno-deprecated -Wno-unused-but-set-variable)
endfunction()

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
endfunction()
