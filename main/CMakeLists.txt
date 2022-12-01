set(self floormat-main)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

add_library(${self}_o STATIC "${sources}")
target_link_libraries(${self}_o PUBLIC
    Magnum::Sdl2Application
    Corrade::Containers
    Magnum::GL
    Magnum::Trade
    fmt::fmt
)

add_library(${self} STATIC dummy.cc)
target_link_libraries(${self} PUBLIC ${self}_o floormat-draw floormat)

if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()
