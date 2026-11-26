if(NOT DEFINED OpenCV_FOUND)
    find_package(OpenCV QUIET)
endif()
if(OpenCV_FOUND)
    set(self "floormat-jxl-encoder")

    file(GLOB sources CONFIGURE_DEPENDS "*.cpp")

    add_library(${self} STATIC "${sources}")
    target_include_directories(${self} SYSTEM PUBLIC "${OpenCV_INCLUDE_DIRS}")
    target_link_libraries(${self} PUBLIC opencv_core floormat-common Corrade::Utility Magnum::Magnum PRIVATE opencv_imgproc jxl jxl_threads)
endif()
