find_package(OpenCV QUIET REQUIRED COMPONENTS core imgcodecs imgproc)

file(GLOB sources "*.cpp" CONFIGURE_ARGS)
set(self "${PROJECT_NAME}-anim")
add_library(${self} STATIC ${sources})

target_include_directories(${self} SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${self} opencv_imgproc opencv_imgcodecs opencv_core)
target_link_libraries(${self} Magnum::Magnum)
