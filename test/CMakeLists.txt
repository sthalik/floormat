set(self floormat-test)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test")

link_libraries(Magnum::GL Magnum::Trade)
link_libraries(floormat-loader floormat-serialize floormat)

if(APPLE)
    link_libraries(Magnum::WindowlessCglApplication)
elseif(WIN32)
    link_libraries(Magnum::WindowlessWglApplication)
    link_libraries(ntdll)
else()
    link_libraries(Magnum::WindowlessGlxApplication)
endif()

add_executable(${self} "${sources}")
if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()
install(TARGETS ${self} RUNTIME DESTINATION bin)
