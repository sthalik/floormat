set(self floormat-loader)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_library(${self}_o STATIC "${sources}")
target_link_libraries(
    ${self}_o PUBLIC
    Magnum::Magnum
    Magnum::Trade
    nlohmann_json::nlohmann_json
)
if(WIN32 OR MAGNUM_BUILD_PLUGINS_STATIC)
    target_link_libraries(${self}_o PUBLIC
        MagnumPlugins::StbImageImporter
        Magnum::TgaImporter
    )
endif()
if(WIN32)
    target_link_libraries(${self}_o PUBLIC ntdll)
endif()

add_library(${self} STATIC dummy.cc)
target_link_libraries(${self} PUBLIC ${self}_o floormat-serialize floormat)

if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()
