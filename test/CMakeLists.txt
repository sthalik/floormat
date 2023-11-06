set(self floormat-test)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test")

add_library(${self}_o OBJECT "${sources}")
target_link_libraries(${self}_o PUBLIC
    ${floormat_headless-library}
    Magnum::GL Magnum::Trade
    nlohmann_json::nlohmann_json
    fmt::fmt tsl::robin_map
)

add_executable(${self} dummy.cc)
target_link_libraries(${self} ${self}_o floormat-serialize floormat)

if (FLOORMAT_ASAN)
    set_target_properties(${self} PROPERTIES OUTPUT_NAME "floormat-test-asan")
endif()

fm_add_install_executable(${self})

file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/save/*.dat" CONFIGURE_ARGS)
foreach(file ${files})
    install(FILES "${file}" DESTINATION "${CMAKE_BINARY_DIR}/test/save")
endforeach()

file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/json/*.json" CONFIGURE_ARGS)
foreach(file ${files})
    install(FILES "${file}" DESTINATION "${CMAKE_BINARY_DIR}/test/json")
endforeach()
