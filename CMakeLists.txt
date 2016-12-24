set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/)
cmake_minimum_required(VERSION 3.0)
project(floormat)
include(opentrack-check-build-directory)
include(opentrack-build)
include(opentrack-platform)

set(RANGES_CXX_STD 14)
include(find-range-v3)

include_directories("${range-v3_INCLUDE_DIR}")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_DEFAULT 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(opentrack-qt)

add_subdirectory(backend)
add_subdirectory(frontend)
