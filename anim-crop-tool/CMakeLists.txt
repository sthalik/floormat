find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    set(self "${PROJECT_NAME}-anim-crop-tool")

    include_directories(SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
    link_libraries(Corrade::Utility Magnum::Magnum)
    link_libraries(opencv_imgproc opencv_imgcodecs opencv_core)
    link_libraries(${PROJECT_NAME})

    file(GLOB sources "*.cpp" CONFIGURE_ARGS)
    add_executable(${self} ${sources})
    install(TARGETS ${self} RUNTIME DESTINATION "bin")
endif()
