set(self floormat)
file(GLOB sources *.cpp ../shaders/*.cpp ../compat/*.cpp CONFIGURE_ARGS)
add_library(${self} OBJECT "${sources}")
target_link_libraries(
    ${self} PUBLIC
    Magnum::GL
    Magnum::Magnum
    Magnum::Shaders
    fmt::fmt
)
if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()
