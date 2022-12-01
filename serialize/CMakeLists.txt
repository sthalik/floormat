set(self floormat-serialize)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_library(${self}_o STATIC "${sources}")
target_link_libraries(
    ${self}_o PUBLIC
    Magnum::Magnum
    nlohmann_json::nlohmann_json
)
add_library(${self} STATIC dummy.cc)
target_link_libraries(${self} PUBLIC ${self}_o floormat-loader floormat)

if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self}_o PRIVATE precomp.hpp)
endif()

if(MSVC)
    add_compile_options(-EHsc)
endif()
