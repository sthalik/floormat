if(NOT DEFINED OpenCV_FOUND)
    find_package(OpenCV QUIET)
endif()
if(OpenCV_FOUND)
    set(self "floormat-wall-tileset-tool")

    include_directories(SYSTEM "${OpenCV_INCLUDE_DIRS}")
    link_libraries(Corrade::Containers Corrade::Utility Magnum::Magnum Magnum::GL)
    link_libraries(opencv_imgproc opencv_imgcodecs opencv_core)
    link_libraries(nlohmann_json::nlohmann_json)
    link_libraries(floormat-common)

    file(GLOB sources CONFIGURE_DEPENDS "*.cpp")

    add_executable(${self} "${sources}")
    target_link_libraries(${self} PRIVATE floormat-serialize floormat floormat-hash)

    fm_win32_app_manifest(${self})
    fm_install_executable(${self})
endif()
