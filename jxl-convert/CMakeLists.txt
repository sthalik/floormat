if(NOT DEFINED OpenCV_FOUND)
    find_package(OpenCV QUIET)
endif()
if(OpenCV_FOUND AND TARGET floormat-jxl-encoder)
    set(self "floormat-jxl-convert")
    add_executable(${self} main.cpp)
    target_include_directories(${self} SYSTEM PRIVATE "${OpenCV_INCLUDE_DIRS}")
    target_link_libraries(${self} PRIVATE
        floormat-jxl-encoder floormat-serialize floormat floormat-hash
        opencv_imgproc opencv_core Jxl::Jxl Magnum::Magnum Corrade::Utility)
    fm_install_executable(${self})
endif()
