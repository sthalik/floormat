set(self floormat-test)
file(GLOB sources CONFIGURE_DEPENDS "*.cpp")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test")

add_library(${self}_o OBJECT "${sources}")
target_link_libraries(${self}_o PUBLIC
    ${floormat_headless-library}
    Magnum::GL Magnum::Trade
    nlohmann_json::nlohmann_json
    floormat-common
    floormat-hwy
)
# test_fpclassify checks infinity().
fm_no_fast_math(FINITE_MATH_ONLY SOURCE float.cpp)

add_executable(${self} dummy.cc)
target_link_libraries(${self} ${self}_o floormat-shader-res floormat-serialize floormat floormat-hash)

fm_win32_app_manifest(${self})
fm_install_executable(${self})

file(GLOB files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/save/*.dat")
foreach(file ${files})
    install(FILES "${file}" DESTINATION "${CMAKE_BINARY_DIR}/test/save")
endforeach()

file(GLOB files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/json/*.json")
foreach(file ${files})
    install(FILES "${file}" DESTINATION "${CMAKE_BINARY_DIR}/test/json")
endforeach()
