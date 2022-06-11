find_package(OpenCV QUIET REQUIRED COMPONENTS core imgcodecs imgproc)
set(self "${PROJECT_NAME}-crop-tool")
file(GLOB sources "*.cpp" CONFIGURE_ARGS)

add_executable(${self} ${sources})
target_include_directories(${self} SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${self} Corrade::Utility)
target_link_libraries(${self} game-anim)
