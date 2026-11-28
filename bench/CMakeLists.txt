set(self floormat-benchmark)

file(GLOB sources CONFIGURE_DEPENDS "*.cpp")

find_package(Threads QUIET REQUIRED)
add_library(${self}_o OBJECT "${sources}")
target_sources(${self}_o PRIVATE "../editor/mazes.cpp" "../editor/torture-test.cpp")
if(NOT MSVC)
    # The same timed code took up to 1.49 times as long at another offset mod 64.
    target_compile_options(${self}_o PRIVATE -falign-functions=64)
endif()
add_executable(${self} dummy.cc)
if(NOT MSVC AND CMAKE_CXX_COMPILER_LINKER_ID STREQUAL "LLD")
    # Aligns the library functions LTO compiles into this exe too.
    target_link_options(${self} PRIVATE "LINKER:-mllvm,-align-all-functions=6")
endif()

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
