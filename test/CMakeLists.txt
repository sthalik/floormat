set(self floormat-test)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test")

add_library(${self}_o OBJECT "${sources}")
target_link_libraries(${self}_o PUBLIC Magnum::GL Magnum::Trade nlohmann_json::nlohmann_json fmt::fmt tsl::robin_map)

if(APPLE)
    target_link_libraries(${self}_o PUBLIC Magnum::WindowlessCglApplication)
elseif(WIN32)
    target_link_libraries(${self}_o PUBLIC Magnum::WindowlessWglApplication ntdll)
else()
    target_link_libraries(${self}_o PUBLIC Magnum::WindowlessGlxApplication)
endif()

add_executable(${self} dummy.cc)
target_link_libraries(${self} ${self}_o floormat-serialize floormat)

install(TARGETS ${self} RUNTIME DESTINATION bin)

set(save-dir "${CMAKE_BINARY_DIR}/test/save")
file(REMOVE_RECURSE "${save-dir}")
file(MAKE_DIRECTORY "${save-dir}")
file(GLOB saves "save/*.dat" CONFIGURE_ARGS)
foreach(file ${saves})
    configure_file("${file}" "${save-dir}" COPYONLY)
endforeach()

#if(NOT MSVC)
#    target_precompile_headers(${self}_o REUSE_FROM floormat)
#else()
#    target_precompile_headers(${self}_o PRIVATE ../src/precomp.hpp)
#endif()
