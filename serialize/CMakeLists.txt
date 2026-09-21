set(self floormat-serialize)

if(MSVC)
    add_compile_options(-EHsc)
endif()

file(GLOB sources "*.cpp" "../loader/*.cpp" CONFIGURE_ARGS)
add_library(${self} OBJECT "${sources}")
target_include_directories(${self} SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../external/magnum-plugins/src/external/stb")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../loader/stb-zlib.cpp" APPEND PROPERTY COMPILE_OPTIONS "-w")
target_link_libraries(
    ${self} PUBLIC
    Magnum::Magnum
    Magnum::Trade
    nlohmann_json::nlohmann_json
    floormat-common
)

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
