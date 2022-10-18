set(self "${PROJECT_NAME}-main")
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-draw)
link_libraries(Magnum::Sdl2Application Magnum::Trade)
link_libraries(MagnumIntegration::ImGui)

corrade_add_resource(res ../resources.conf)
add_executable(${self} "${sources}" "${res}")
set_property(TARGET ${self} PROPERTY OUTPUT_NAME "${PROJECT_NAME}")
install(TARGETS ${self} RUNTIME DESTINATION bin)
