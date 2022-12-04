set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(floormat C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_DEFAULT 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_DEFAULT 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS FALSE)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_C_LINKER_PREFERENCE_PROPAGATES OFF)
set(CMAKE_CXX_LINKER_PREFERENCE_PROPAGATES ON)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

set(CMAKE_INSTALL_MESSAGE LAZY)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_SKIP_RPATH FALSE)

set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0068 NEW)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR CMAKE_INSTALL_PREFIX STREQUAL "")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY AND NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY AND NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/bin" CACHE PATH "" FORCE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/lib" CACHE PATH "" FORCE)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/lib" CACHE PATH "" FORCE)
endif()

if(NOT GENERATOR_IS_MULTI_CONFIG)
if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "RELEASE" CACHE STRING "" FORCE)
endif()
endif()

set(FLOORMAT_PRECOMPILED-HEADERS ON CACHE BOOL
    "Use precompiled headers while building the floormat.")

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

if(NOT GENERATOR_IS_MULTI_CONFIG)
string(TOUPPER "${CMAKE_BUILD_TYPE}" __fm_uppercase_CMAKE_BUILD_TYPE)
sets(STRING CMAKE_BUILD_TYPE "${__fm_uppercase_CMAKE_BUILD_TYPE}")
endif()

macro(fm_run_hook str)
    if(COMMAND "${str}")
        cmake_language(CALL "${str}")
    endif()
endmacro()

macro(fm_load_userconfig)
    if(NOT DEFINED FLOORMAT_USERCONFIG)
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            set(ENV{USER} "$ENV{USERNAME}")
        endif()
        sets(FILEPATH FLOORMAT_USERCONFIG "${CMAKE_CURRENT_SOURCE_DIR}/userconfig-$ENV{USER}@${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}.cmake")
    endif()
    if(EXISTS "${FLOORMAT_USERCONFIG}")
        message(STATUS "Loading user config '${FLOORMAT_USERCONFIG}'")
        include("${FLOORMAT_USERCONFIG}" NO_POLICY_SCOPE)
    else()
        message(STATUS "Can't find user config '${FLOORMAT_USERCONFIG}'")
    endif()
endmacro()

if(WIN32)
    if(MSVC)
        set(CMAKE_RC_FLAGS "/nologo /DWIN32")
    endif()
    enable_language(RC)
endif()

if(NOT "$ENV{FLOORMAT_NO_USERCONFIG}" AND NOT FLOORMAT_NO_USERCONFIG)
    fm_load_userconfig()
    if(NOT GENERATOR_IS_MULTI_CONFIG)
        string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)
    endif()
endif()

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_HAS_EXCEPTIONS=0)
endif()

if(MSVC)
    add_definitions(-D_USE_MATH_DEFINES=1)
    add_compile_options(-permissive-)
    add_compile_options(-Zc:preprocessor -Zc:inline -Zc:lambda -Zc:throwingNew -Zc:__cplusplus)
    add_compile_options(-external:W0 -external:anglebrackets)
    add_compile_options(
        #-wd4244 # warning C4244: 'argument': conversion from 'int' to 'float', possible loss of data
        #-wd4251 # warning C4251: 't::f': class 'x' needs to have dll-interface to be used by clients of class 'y'
        -wd4456  # warning C4456: declaration of 'x' hides previous local declaration
        -wd4458  # warning C4458: declaration of 'keys' hides class member
        -wd4127  # warning C4127: conditional expression is constant
        -wd4554  # warning C4554: '<<': check operator precedence for possible error; use parentheses to clarify precedence
        -wd4146  # warning C4146: unary minus operator applied to unsigned type, result still unsigned
    )
    add_definitions(-utf-8)
    if(CMAKE_SIZEOF_VOID_P GREATER_EQUAL 8)
        add_link_options(-HIGHENTROPYVA)
    endif()
else()
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fuse-cxa-atexit>)
    if(WIN32)
        add_link_options(-Wl,--nxcompat -Wl,--dynamicbase)
        if(CMAKE_SIZEOF_VOID_P GREATER_EQUAL 8)
            add_link_options(-Wl,--high-entropy-va)
        else()
            add_link_options(-Wl,--large-address-aware)
        endif()
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL "13.3" OR
   CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL "13.0")
    add_compile_options(-Wno-reserved-macro-identifier)
endif()

set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS OFF)
set_directory_properties(PROPERTIES CORRADE_CXX_STANDARD ${CMAKE_CXX_STANDARD})
set_directory_properties(PROPERTIES INTERFACE_CORRADE_CXX_STANDARD ${CMAKE_CXX_STANDARD})

add_subdirectory(external)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/FIcompat/prelude.hpp>)
else()
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-includecompat/prelude.hpp>)
endif()

add_definitions(
    -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS
    -DIMGUI_DISABLE_OBSOLETE_KEYIO
)

if(NOT APPLE AND NOT WIN32)
    add_compile_options(-fno-plt)
endif()

if(MSVC)
    add_compile_options(-GR-)
else()
    add_compile_options(-fno-rtti)
endif()

fm_run_hook(fm-userconfig-src)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang$")
    add_definitions(-D_LIBCPP_REMOVE_TRANSITIVE_INCLUDES)
    add_compile_options(-Wno-shift-op-parentheses)
elseif(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-Wno-subobject-linkage -Wno-parentheses)
endif()

include_directories(.)
include_directories(src)

#get_property(_fm-sdl2-include-dirs TARGET SDL2::SDL2 PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
get_property(_fm-json-include-dirs TARGET nlohmann_json::nlohmann_json PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
include_directories(SYSTEM
    "${CORRADE_INCLUDE_DIR}"
    "${MAGNUM_INCLUDE_DIR}"
    "${MAGNUMPLUGINS_INCLUDE_DIR}"
    "${MAGNUMINTEGRATION_INCLUDE_DIR}"
    #"${_fm-sdl2-include-dirs}"
    "${_fm-json-include-dirs}"
)

#add_subdirectory(demangle)
add_subdirectory(src)
add_subdirectory(serialize)
add_subdirectory(anim-crop-tool)
add_subdirectory(draw)
add_subdirectory(main)
add_subdirectory(editor)
add_subdirectory(test)

install(DIRECTORY images anim scenery DESTINATION "share/floormat")

fm_run_hook(fm-userconfig-post)
get_cmake_property(_variableNames VARIABLES)
