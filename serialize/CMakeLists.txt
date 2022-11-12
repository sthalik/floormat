set(self floormat-serialize)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_library(${self} STATIC "${sources}")
target_link_libraries(
    ${self} PUBLIC
    floormat-loader
    floormat
    Magnum::Magnum
    nlohmann_json::nlohmann_json
)
if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()

if(MSVC)
    add_compile_options(-EHsc)
endif()
