set(self floormat)
file(GLOB sources *.cpp ../shaders/*.cpp ../compat/*.cpp ../entity/*.cpp CONFIGURE_ARGS)
add_library(${self} OBJECT "${sources}")
target_link_libraries(
    ${self} PUBLIC
    Magnum::GL
    Magnum::Magnum
    Magnum::Shaders
    fmt::fmt
    tsl::robin_map
)

target_precompile_headers(${self} PRIVATE precomp.hpp)
