set(self ${PROJECT_NAME}-main)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

link_libraries(${PROJECT_NAME})
link_libraries(Magnum::Sdl2Application Corrade::Containers Magnum::GL Magnum::Trade fmt::fmt)

add_library(${self} STATIC "${sources}")

if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()
