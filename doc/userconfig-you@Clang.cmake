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

add_compile_options(-emit-llvm)
sets(STRING
     CMAKE_C_FLAGS ""
     CMAKE_C_FLAGS_DEBUG "-O0 -g -glldb"
     CMAKE_C_FLAGS_RELEASE "-O2 -mtune=native"
)
sets(STRING
     CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}"
     CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}"
     CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}"
)

add_compile_options(-fdiagnostics-color=always)

# for building submodule dependencies
function(fm-userconfig-external-pre)
    add_compile_options(
        -Wno-ignored-attributes
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-old-style-cast
    )
endfunction()

# for floormat sources only
function(fm-userconfig-src)
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wno-old-style-cast -Wno-padded
    )
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Weverything
        -Wno-c++98-compat
        -Wno-c++20-compat
        -Wno-c++98-compat-pedantic
        -Wno-undefined-func-template
        -Wno-switch-enum
        -Wno-covered-switch-default
        -Wno-old-style-cast
        -Wno-global-constructors
        -Wno-exit-time-destructors
        -Wno-implicit-int-float-conversion
        -Wno-shadow-field-in-constructor
        -Wno-shadow
        -Wno-ctad-maybe-unsupported
        -Wno-documentation-unknown-command
        -Wno-documentation
        -Wno-ignored-attributes
        -Wno-reserved-identifier
    )
    add_compile_options(
        -Werror
        -Wno-error=float-equal
        -Wno-error=comma
        -Wno-error=unused-parameter
        -Wno-error=unused-private-field
        -Wno-error=alloca
    )
endfunction()

sets(BOOL
     SDL_SHARED             ON
     SDL_STATIC             OFF    # speed up linking

     CORRADE_BUILD_TESTS    TRUE
     MAGNUM_BUILD_TESTS     TRUE
)

if (FLOORMAT_WITH-COVERAGE OR CMAKE_CXX_BUILD_TYPE STREQUAL "RELEASE")
    sets(BOOL
         CORRADE_BUILD_TESTS    FALSE
         MAGNUM_BUILD_TESTS     FALSE
    )
endif()
