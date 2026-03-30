set(self floormat-benchmark)

file(GLOB sources "*.cpp" CONFIGURE_ARGS)

add_library(${self}_o OBJECT "${sources}")
add_executable(${self} dummy.cc)

target_link_libraries(${self}_o PUBLIC
    ${floormat_headless-library}
    Magnum::Magnum
    Magnum::Trade
    nlohmann_json::nlohmann_json
    fmt::fmt
    tsl::robin_map
    floormat-common
)

if(TARGET benchmark::benchmark)
    target_link_libraries(${self}_o PUBLIC benchmark::benchmark)
else()
    target_link_libraries(${self}_o PUBLIC benchmark)
endif()

target_link_libraries(${self} PUBLIC ${self}_o floormat-serialize floormat-draw floormat floormat-hash)
fm_install_executable(${self})
