find_package(OpenCV QUIET REQUIRED COMPONENTS core imgcodecs imgproc)
find_package(nlohmann_json QUIET REQUIRED)

set(self "${PROJECT_NAME}-tile")

include_directories(SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
link_libraries(opencv_imgproc opencv_imgcodecs opencv_core)
link_libraries(Magnum::Magnum nlohmann_json::nlohmann_json)

file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_library(${self} STATIC ${sources})

