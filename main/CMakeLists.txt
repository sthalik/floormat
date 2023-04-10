set(self floormat-main)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

add_library(${self} OBJECT "${sources}")
target_link_libraries(${self} PUBLIC
    Magnum::Sdl2Application
    Corrade::Containers
    Magnum::GL
    Magnum::Trade
    fmt::fmt
    tsl::robin_map
)
