set(self floormat-loader)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_library(${self} STATIC "${sources}")
target_link_libraries(
    ${self} PUBLIC
    floormat-serialize
    floormat
    Magnum::Magnum
    Magnum::Trade
)
if(WIN32 OR MAGNUM_BUILD_PLUGINS_STATIC)
    target_link_libraries(${self} PUBLIC
        MagnumPlugins::StbImageImporter
        Magnum::TgaImporter
    )
endif()
if(WIN32)
    target_link_libraries(${self} PUBLIC ntdll)
endif()
if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()
