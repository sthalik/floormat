set(self ${PROJECT_NAME})
file(GLOB sources "*.cpp" "../shaders/*.cpp" "../serialize/*.cpp" "../draw/*.cpp" CONFIGURE_ARGS)
add_library(${self} STATIC "${sources}")
target_link_libraries(
    ${self} PUBLIC
    Magnum::GL
    Magnum::Magnum
    Magnum::Shaders
    nlohmann_json::nlohmann_json
)
if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()

if(WIN32 OR MAGNUM_BUILD_STATIC)
    target_link_libraries(${self} PUBLIC
        Magnum::AnyImageImporter
        Magnum::TgaImporter
    )
endif()
