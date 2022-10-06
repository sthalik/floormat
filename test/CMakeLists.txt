set(self "${PROJECT_NAME}-test")
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

link_libraries(${PROJECT_NAME})
link_libraries(Magnum::Application Magnum::Trade)

add_executable(${self} "${sources}" "../main/loader-impl.cpp")
install(TARGETS ${self} RUNTIME DESTINATION bin)
