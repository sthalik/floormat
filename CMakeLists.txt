set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(floormat)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_DEFAULT 23)
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

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

if(MSVC)
    add_definitions(-D_USE_MATH_DEFINES=1)
endif()

if(COMMAND fm_hook_pre_external)
    fm_hook_pre_external()
endif()

set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS OFF)
set_directory_properties(PROPERTIES CORRADE_CXX_STANDARD ${CMAKE_CXX_STANDARD})
set_directory_properties(PROPERTIES INTERFACE_CORRADE_CXX_STANDARD ${CMAKE_CXX_STANDARD})

function(sets type)
    set(i 0)
    list(LENGTH ARGN max)
    math(EXPR foo "${max} % 2")
    if(NOT foo EQUAL 0)
        message(FATAL_ERROR "argument count not even")
    endif()
    while(i LESS max)
        list(GET ARGN "${i}" name)
        math(EXPR i "${i} + 1")
        list(GET ARGN "${i}" value)
        math(EXPR i "${i} + 1")
        set(${name} "${value}" CACHE "${type}" "" FORCE)
    endwhile()
endfunction()

cmake_policy(SET CMP0063 NEW)

if(MSVC)
    add_compile_options(-permissive-)
    add_compile_options(
        -wd4244 # warning C4244: 'argument': conversion from 'int' to 'float', possible loss of data
        -wd4312 # warning C4312: 'reinterpret_cast': conversion from 'GLenum' to 'void *' of greater size
        -wd4251 # warning C4251: 't::f': class 'x' needs to have dll-interface to be used by clients of class 'y'
        -wd4456 # warning C4456: declaration of 'x' hides previous local declaration
    )
    add_compile_options(-external:W0 -external:anglebrackets)
    if(CMAKE_SIZEOF_VOID_P GREATER_EQUAL 8)
        add_link_options(-HIGHENTROPYVA)
    endif()
endif()

set(FLOORMAT_SUBMODULE_DEPENDENCIES ON CACHE BOOL
    "Use dependencies included in the source directory (needs git submodule update --init).")

function(fm_load_userconfig)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(_fm_username "$ENV{USERNAME}")
    else()
        set(_fm_username "$ENV{USER}")
    endif()
    set(_fm_userconfig "userconfig-${_fm_username}@${CMAKE_CXX_COMPILER_ID}.cmake")
    if(EXISTS "${CMAKE_SOURCE_DIR}/${_fm_userconfig}")
        include("${CMAKE_SOURCE_DIR}/${_fm_userconfig}")
    else()
        message(STATUS "user config '${_fm_userconfig}' not found")
    endif()
endfunction()
fm_load_userconfig()

add_subdirectory(external)

include_directories(.)
include_directories(src)

if(MSVC)
    add_definitions(/FI"compat/prelude.hpp")
else()
    add_definitions(-include compat/prelude.hpp)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang$")
    set_property(DIRECTORY APPEND PROPERTY COMPILE_FLAGS -Wignored-attributes -Werror -ferror-limit=5)
endif()

add_subdirectory(src)
add_subdirectory(draw)
add_subdirectory(main)
add_subdirectory(anim-crop-tool)
add_subdirectory(test)

install(DIRECTORY images DESTINATION "share/${PROJECT_NAME}")
