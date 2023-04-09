sets(BOOL FLOORMAT_PRECOMPILED-HEADERS OFF)
sets(BOOL FLOORMAT_SUBMODULE-SDL2 OFF)
set(CMAKE_INSTALL_MESSAGE NEVER)
sets(STRING
     CMAKE_BUILD_TYPE RELEASE
     CMAKE_C_FLAGS_RELEASE "-O0 -g -ggdb -DNDEBUG"
     CMAKE_CXX_FLAGS_RELEASE "-O0 -g -ggdb -DNDEBUG")

add_compile_options(-fsanitize=address,undefined,memory)
add_link_options(-fsanitize=address,undefined,memory)
sets(BOOL CORRADE_CPU_USE_IFUNC OFF)
function(fm-userconfig-src)
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wno-old-style-cast -Wno-padded
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
        -Wno-unsafe-buffer-usage
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
        -Wno-error=comma
    )
endfunction()
