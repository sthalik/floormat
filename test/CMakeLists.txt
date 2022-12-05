set(self floormat-test)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test")

add_library(${self}_o OBJECT "${sources}")
target_link_libraries(${self}_o PUBLIC Magnum::GL Magnum::Trade nlohmann_json::nlohmann_json fmt::fmt)

if(APPLE)
    target_link_libraries(${self}_o PUBLIC Magnum::WindowlessCglApplication)
elseif(WIN32)
    target_link_libraries(${self}_o PUBLIC Magnum::WindowlessWglApplication ntdll)
else()
    target_link_libraries(${self}_o PUBLIC Magnum::WindowlessGlxApplication)
endif()

add_executable(${self} dummy.cc)
target_link_libraries(${self} ${self}_o floormat-serialize floormat)

if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self}_o PRIVATE precomp.hpp)
endif()
install(TARGETS ${self} RUNTIME DESTINATION bin)
