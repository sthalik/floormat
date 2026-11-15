set(self floormat-benchmark)

file(GLOB sources CONFIGURE_DEPENDS "*.cpp")

find_package(Threads QUIET REQUIRED)
add_library(${self}_o OBJECT "${sources}")
target_sources(${self}_o PRIVATE "../editor/mazes.cpp")
add_executable(${self} dummy.cc)

target_link_libraries(${self}_o PUBLIC
    ${floormat_headless-library}
    Magnum::Magnum
    Magnum::Trade
    nlohmann_json::nlohmann_json
    floormat-common
    floormat-hwy
    Threads::Threads
)

if(TARGET benchmark::benchmark)
    target_link_libraries(${self}_o PUBLIC benchmark::benchmark)
else()
    target_link_libraries(${self}_o PUBLIC benchmark)
endif()

target_link_libraries(${self} PUBLIC ${self}_o floormat-serialize floormat-draw floormat floormat-hash floormat-random)
fm_win32_app_manifest(${self})
fm_install_executable(${self})

file(GLOB files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/save/*.dat")
foreach(file ${files})
    install(FILES "${file}" DESTINATION "${CMAKE_BINARY_DIR}/bench/save")
endforeach()
