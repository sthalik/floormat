set(self "${PROJECT_NAME}-test")
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test")

link_libraries(${PROJECT_NAME})
link_libraries(Magnum::WindowlessWglApplication Magnum::Trade)

#add_executable(${self} "${sources}" "../main/loader-impl.cpp")
add_library(${self} STATIC "${sources}" "../main/loader-impl.cpp")
install(TARGETS ${self} RUNTIME DESTINATION bin)
