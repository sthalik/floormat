set(self ${PROJECT_NAME}-draw)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_library(${self} STATIC "${sources}")
target_link_libraries(${self} PUBLIC
    Magnum::GL
    Magnum::Magnum
    Magnum::Shaders
    ${PROJECT_NAME}
)
if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()
