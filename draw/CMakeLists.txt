set(self floormat-draw)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_library(${self} STATIC "${sources}")
target_link_libraries(
    ${self} PUBLIC
    floormat
    Magnum::GL
)
if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()
