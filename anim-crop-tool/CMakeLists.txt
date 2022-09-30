find_package(OpenCV QUIET REQUIRED COMPONENTS core imgcodecs imgproc)
set(self "${PROJECT_NAME}-anim-crop-tool")

include_directories(SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
link_libraries(Corrade::Utility)
link_libraries(${PROJECT_NAME}-anim)

file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_executable(${self} ${sources})
install(TARGETS ${self} RUNTIME DESTINATION "bin")
