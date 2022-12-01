find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    set(self "floormat-anim-crop-tool")

    include_directories(SYSTEM ${OpenCV_INCLUDE_DIRS})
    link_libraries(Corrade::Utility Magnum::Magnum)
    link_libraries(opencv_imgproc opencv_imgcodecs opencv_core)
    link_libraries(nlohmann_json::nlohmann_json)

    file(GLOB sources "*.cpp" CONFIGURE_ARGS)
    add_library(${self}_o STATIC "${sources}")

    add_executable(${self} dummy.cc)
    target_link_libraries(${self} PUBLIC ${self}_o floormat-serialize floormat)

    install(TARGETS ${self} RUNTIME DESTINATION "bin")
endif()
