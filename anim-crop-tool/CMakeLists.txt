find_package(OpenCV QUIET REQUIRED COMPONENTS core imgcodecs imgproc)
set(self "${PROJECT_NAME}-anim-crop-tool")

include_directories(SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
link_libraries(Corrade::Utility Magnum::Magnum)
link_libraries("${PROJECT_NAME}-serialize")
link_libraries(opencv_imgproc opencv_imgcodecs opencv_core)

file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_executable(${self} ${sources})
install(TARGETS ${self} RUNTIME DESTINATION "bin")
