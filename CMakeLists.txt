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

set(CMAKE_INSTALL_MESSAGE LAZY)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR CMAKE_INSTALL_PREFIX STREQUAL "")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()

if(MSVC)
    add_compile_options(-EHsc)
endif()

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES=1 -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS OFF)
set_directory_properties(PROPERTIES CORRADE_CXX_STANDARD 20)
set_directory_properties(PROPERTIES INTERFACE_CORRADE_CXX_STANDARD 20)

set(_userconfig "userconfig-${CMAKE_CXX_COMPILER_ID}.cmake")
if(EXISTS "${CMAKE_SOURCE_DIR}/${_userconfig}")
    include("${CMAKE_SOURCE_DIR}/${_userconfig}" NO_POLICY_SCOPE)
else()
    message(STATUS "user config '${_userconfig}' not found")
endif()

if(NOT DEFINED BOOTSTRAP_DEPENDS)
    set(BOOTSTRAP_DEPENDS "$ENV{BOOTSTRAP_DEPENDS}")
endif()

if(NOT BOOTSTRAP_DEPENDS)
    find_package(Corrade QUIET REQUIRED)
    find_package(Magnum QUIET REQUIRED COMPONENTS GL Shaders Trade Sdl2Application)
    find_package(MagnumPlugins QUIET REQUIRED)
    find_package(MagnumIntegration QUIET REQUIRED COMPONENTS Glm)

    include_directories(.)
    include_directories(src)

    add_subdirectory(src)
    add_subdirectory(main)
    add_subdirectory(anim-crop-tool)
    add_subdirectory(test)

    install(DIRECTORY images DESTINATION "share/${PROJECT_NAME}")
endif()
