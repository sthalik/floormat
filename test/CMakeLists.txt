set(self "${PROJECT_NAME}-test")
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test")

link_libraries(${PROJECT_NAME})
link_libraries(Magnum::Trade)

if(APPLE)
    link_libraries(Magnum::WindowlessCglApplication)
elseif(WIN32)
    link_libraries(Magnum::WindowlessWglApplication)
else()
    link_libraries(Magnum::WindowlessGlxApplication)
endif()

add_executable(${self} "${sources}" "../loader/loader-impl.cpp")
install(TARGETS ${self} RUNTIME DESTINATION bin)
