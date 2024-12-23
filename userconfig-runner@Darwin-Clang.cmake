sets(BOOL FLOORMAT_SUBMODULE-SDL2 OFF)
set(CMAKE_INSTALL_MESSAGE NEVER)
sets(STRING
     CMAKE_BUILD_TYPE RELEASE
     CMAKE_C_FLAGS_RELEASE "-O0 -DNDEBUG"
     CMAKE_CXX_FLAGS_RELEASE "-O0 -DNDEBUG")

add_link_options(-framework IOKit)

# for building submodule dependencies
function(fm-userconfig-external)
    add_compile_options(-Wno-deprecated -Wno-unused-but-set-variable -Wno-poison-system-directories)
endfunction()

# for floormat sources only
function(fm-userconfig-src)
    # start copy-paste
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
        -Wno-bitwise-op-parentheses
        -Wno-weak-vtables
        -Wno-c99-compat
        -Wno-switch-default
    )
    #end copy-paste
    add_compile_options(-Wno-poison-system-directories)
endfunction()
