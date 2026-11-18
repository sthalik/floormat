# libjxl's hwy target is defined in external/CMakeLists.txt from external/highway.
if(TARGET hwy)
    set(HWY_FOUND TRUE)
else()
    find_package(HWY CONFIG QUIET)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(HWY CONFIG_MODE)
endif()
