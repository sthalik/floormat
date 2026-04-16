set(self floormat-serialize)
file(GLOB sources "*.cpp" "../loader/*.cpp" CONFIGURE_ARGS)
add_library(${self} OBJECT "${sources}")
target_link_libraries(
    ${self} PUBLIC
    Magnum::Magnum
    Magnum::Trade
    nlohmann_json::nlohmann_json
    floormat-common
)

if(MSVC)
    add_compile_options(-EHsc)
endif()

if(WIN32 OR MAGNUM_BUILD_PLUGINS_STATIC)
    target_link_libraries(${self} PUBLIC
        MagnumPlugins::StbImageImporter
        MagnumPlugins::StbImageConverter
        Magnum::AnyImageConverter
        Magnum::TgaImporter
    )
endif()
if(WIN32)
    target_link_libraries(${self} PUBLIC ntdll)
endif()
