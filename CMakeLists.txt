set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/)
cmake_minimum_required(VERSION 2.8.2)
set_property(GLOBAL PROPERTY USE_FOLDERS OFF)
project(floormat)
include(opentrack-check-build-directory)
include(opentrack-platform NO_POLICY_SCOPE)
include(opentrack-build)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_DEFAULT 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -source-charset:UTF-8")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -finput-charset=UTF-8")
endif()

include(opentrack-qt)

add_subdirectory(backend)
add_subdirectory(frontend)
