set(self "${PROJECT_NAME}-main")
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

link_libraries(${PROJECT_NAME}
               Magnum::Application Magnum::Trade MagnumIntegration::Glm)
corrade_add_resource(res ../resources.conf)
add_executable(${self} WIN32 "${sources}" "${res}")
set_property(TARGET ${self} PROPERTY OUTPUT_NAME "${PROJECT_NAME}")
