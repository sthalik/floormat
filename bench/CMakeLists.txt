set(self floormat-benchmark)

file(GLOB sources CONFIGURE_DEPENDS "*.cpp")

add_library(${self}_o OBJECT "${sources}")
add_executable(${self} dummy.cc)

target_link_libraries(${self}_o PUBLIC
    ${floormat_headless-library}
    Magnum::Magnum
    Magnum::Trade
    nlohmann_json::nlohmann_json
    floormat-common
)

if(TARGET benchmark::benchmark)
    target_link_libraries(${self}_o PUBLIC benchmark::benchmark)
else()
    target_link_libraries(${self}_o PUBLIC benchmark)
endif()

# Same ±inf ray direction as src/raycast.cpp.
fm_no_fast_math(FINITE_MATH_ONLY SOURCE raycast.cpp)

target_link_libraries(${self} PUBLIC ${self}_o floormat-serialize floormat-draw floormat floormat-hash)
fm_win32_app_manifest(${self})
fm_install_executable(${self})
