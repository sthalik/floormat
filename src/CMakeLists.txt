set(self floormat)
file(GLOB sources "*.cpp" "../shaders/*.cpp" CONFIGURE_ARGS)
add_library(${self} STATIC "${sources}")
target_link_libraries(
    ${self} PUBLIC
    Magnum::GL
    Magnum::Magnum
    Magnum::Shaders
)
if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()

if(WIN32 OR MAGNUM_BUILD_PLUGINS_STATIC)
    target_link_libraries(${self} PUBLIC
        MagnumPlugins::StbImageImporter
        Magnum::TgaImporter
    )
endif()
