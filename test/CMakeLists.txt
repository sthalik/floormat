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

install(TARGETS ${self} RUNTIME DESTINATION bin)
fm_add_debug_info(${self})

set(save-dir "${CMAKE_BINARY_DIR}/test/save")
file(REMOVE_RECURSE "${save-dir}")
file(MAKE_DIRECTORY "${save-dir}")
file(GLOB saves "save/*.dat" CONFIGURE_ARGS)
foreach(file ${saves})
    configure_file("${file}" "${save-dir}" COPYONLY)
endforeach()
