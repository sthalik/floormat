set(self floormat-main)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

add_library(${self} OBJECT "${sources}")
target_link_libraries(${self} PUBLIC
    Magnum::Sdl2Application
    Corrade::Containers
    Magnum::GL
    Magnum::Trade
    floormat-common
)

if(NOT TARGET SDL2::SDL2)
    find_package(SDL2 QUIET REQUIRED)
endif()
target_link_libraries(${self} PRIVATE SDL2::SDL2)
