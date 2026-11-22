set(self floormat-benchmark)

file(GLOB sources CONFIGURE_DEPENDS "*.cpp")

find_package(Threads QUIET REQUIRED)
add_library(${self}_o OBJECT "${sources}")
target_sources(${self}_o PRIVATE "../editor/mazes.cpp" "../editor/torture-test.cpp")
add_executable(${self} dummy.cc)

set(libs
    ${floormat_headless-library}
    Magnum::Magnum
    Magnum::Trade
    nlohmann_json::nlohmann_json
    floormat-common
    floormat-hwy
    Threads::Threads
)
if(TARGET benchmark::benchmark)
    list(APPEND libs benchmark::benchmark)
else()
    list(APPEND libs benchmark)
endif()
target_link_libraries(${self}_o PUBLIC ${libs})

target_link_libraries(${self} PUBLIC ${self}_o floormat-serialize floormat-draw floormat floormat-hash floormat-random)
fm_win32_app_manifest(${self})
fm_install_executable(${self})

add_executable(${self}-slab-large-heap EXCLUDE_FROM_ALL main.cpp slab-allocator.cpp)
target_compile_definitions(${self}-slab-large-heap PRIVATE FM_SLAB_BENCH_LARGE_HEAP)
target_link_libraries(${self}-slab-large-heap PRIVATE
    ${libs} floormat-serialize floormat-draw floormat floormat-hash floormat-random)
fm_win32_app_manifest(${self}-slab-large-heap)

file(GLOB files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/save/*.dat")
foreach(file ${files})
    install(FILES "${file}" DESTINATION "${CMAKE_BINARY_DIR}/bench/save")
endforeach()
