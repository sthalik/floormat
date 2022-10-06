set(self "${PROJECT_NAME}-main")
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

link_libraries(${PROJECT_NAME})
link_libraries(Magnum::Application Magnum::Trade)

corrade_add_resource(res ../resources.conf)
add_executable(${self} WIN32 "${sources}" "${res}")
set_property(TARGET ${self} PROPERTY OUTPUT_NAME "${PROJECT_NAME}")
install(TARGETS ${self} RUNTIME DESTINATION bin)
