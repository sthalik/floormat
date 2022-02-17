list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(game)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_DEFAULT 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_DEFAULT 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS FALSE)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_LINKER_PREFERENCE_PROPAGATES OFF)
set(CMAKE_CXX_LINKER_PREFERENCE_PROPAGATES ON)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR CMAKE_INSTALL_PREFIX STREQUAL "")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()

set(_userconfig "userconfig-${CMAKE_CXX_COMPILER_ID}.cmake")
if(EXISTS "${CMAKE_SOURCE_DIR}/${_userconfig}")
    include("${CMAKE_SOURCE_DIR}/${_userconfig}")
else()
    message(STATUS "user config '${_userconfig}' not found")
endif()

if(MSVC)
    add_compile_options(-EHsc)
endif()

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES=1 -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

include_directories("glm")

find_package(SDL2 QUIET)
find_package(Corrade REQUIRED)
find_package(Magnum REQUIRED GL Shaders WglContext Sdl2Application Trade)

set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS OFF)
set_directory_properties(PROPERTIES CORRADE_CXX_STANDARD 20)
set_directory_properties(PROPERTIES INTERFACE_CORRADE_CXX_STANDARD 20)

corrade_add_resource(TexturedQuad_RESOURCES resources.conf)

file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_executable(magnum-triangle WIN32  "${sources}" "${TexturedQuad_RESOURCES}")
target_link_libraries(magnum-triangle PRIVATE
                      Magnum::Application
                      Magnum::GL
                      Magnum::WglContext
                      Magnum::Magnum
                      Magnum::Shaders
                      Magnum::Trade)

install(TARGETS DESTINATION .)
