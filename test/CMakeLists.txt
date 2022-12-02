set(self floormat-test)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test")

add_executable(${self} "${sources}")

target_link_libraries(${self} PRIVATE floormat-serialize floormat)
target_link_libraries(${self} PRIVATE Magnum::GL Magnum::Trade nlohmann_json::nlohmann_json)

if(APPLE)
    target_link_libraries(${self} PRIVATE Magnum::WindowlessCglApplication)
elseif(WIN32)
    target_link_libraries(${self} PRIVATE Magnum::WindowlessWglApplication)
    target_link_libraries(${self} PRIVATE ntdll)
else()
    target_link_libraries(${self} PRIVATE Magnum::WindowlessGlxApplication)
endif()

if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()
install(TARGETS ${self} RUNTIME DESTINATION bin)
