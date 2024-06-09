if(NOT DEFINED OpenCV_FOUND)
    find_package(OpenCV QUIET)
endif()
if(OpenCV_FOUND)
    set(self "floormat-wall-tileset-tool")

    include_directories(SYSTEM "${OpenCV_INCLUDE_DIRS}")
    link_libraries(Corrade::Containers Corrade::Utility Magnum::Magnum Magnum::GL fmt::fmt)
    link_libraries(opencv_imgproc opencv_imgcodecs opencv_core)
    link_libraries(nlohmann_json::nlohmann_json)

    file(GLOB sources "*.cpp" CONFIGURE_ARGS)

    add_executable(${self} "${sources}")
    target_link_libraries(${self} PRIVATE floormat-serialize floormat floormat-hash)

    fm_install_executable(${self})
endif()
