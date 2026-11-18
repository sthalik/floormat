#.rst:
# Find Jxl
# --------
#
# Finds the libjxl library. This module defines:
#
#  Jxl_FOUND                - True if the libjxl library is found
#  Jxl::Jxl                 - libjxl imported target
#  Jxl::Threads             - libjxl thread pool imported target
#
# Additionally these variables are defined for internal usage:
#
#  Jxl_LIBRARY              - libjxl library
#  Jxl_THREADS_LIBRARY      - libjxl thread pool library
#  Jxl_INCLUDE_DIR          - Include dir
#
# If libjxl is a static library, its dependencies are linked to Jxl::Jxl and
# these variables are defined as well:
#
#  Jxl_CMS_LIBRARY          - libjxl color management library, present
#   since libjxl 0.9
#  Jxl_HWY_LIBRARY          - Highway library
#  Jxl_BROTLIENC_LIBRARY    - Brotli encoder library
#  Jxl_BROTLIDEC_LIBRARY    - Brotli decoder library
#  Jxl_BROTLICOMMON_LIBRARY - Brotli common library
#
# If the thread pool library is static, Jxl::Threads links to Threads::Threads.
#

#
#   This file is part of Magnum.
#
#   Copyright © 2026 Stanisław Halik <sthalik@misaki.pl>
#
#   Permission is hereby granted, free of charge, to any person obtaining a
#   copy of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom the
#   Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.
#

if(TARGET jxl)
    if(NOT TARGET Jxl::Jxl)
        add_library(Jxl::Jxl INTERFACE IMPORTED)
        set_target_properties(Jxl::Jxl PROPERTIES INTERFACE_LINK_LIBRARIES jxl)
    endif()
    if(TARGET jxl_threads AND NOT TARGET Jxl::Threads)
        add_library(Jxl::Threads INTERFACE IMPORTED)
        set_target_properties(Jxl::Threads PROPERTIES INTERFACE_LINK_LIBRARIES jxl_threads)
    endif()

    set(_JXL_TARGET jxl)
    if(TARGET jxl_threads)
        set(_JXL_THREADS_TARGET jxl_threads)
    endif()
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Jxl DEFAULT_MSG _JXL_TARGET _JXL_THREADS_TARGET)
    return()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND AND NOT CMAKE_VERSION VERSION_LESS 3.6)
    pkg_check_modules(PC_Jxl QUIET IMPORTED_TARGET libjxl)
    pkg_check_modules(PC_JxlThreads QUIET IMPORTED_TARGET libjxl_threads)
endif()

find_library(Jxl_LIBRARY NAMES jxl jxl-static libjxl
    HINTS ${PC_Jxl_LIBRARY_DIRS})
find_library(Jxl_THREADS_LIBRARY NAMES jxl_threads jxl_threads-static libjxl_threads
    HINTS ${PC_JxlThreads_LIBRARY_DIRS})
find_path(Jxl_INCLUDE_DIR NAMES jxl/decode.h
    HINTS ${PC_Jxl_INCLUDE_DIRS})

foreach(_jxl_library Jxl_LIBRARY Jxl_THREADS_LIBRARY)
    set(_${_jxl_library}_IS_STATIC OFF)
    if(${_jxl_library} MATCHES "\.a$" AND NOT ${_jxl_library} MATCHES "\.dll\.a$")
        set(_${_jxl_library}_IS_STATIC ON)
    elseif(${_jxl_library} MATCHES "\.lib$")
        file(STRINGS ${${_jxl_library}} _JXL_IMPORT_DESCRIPTOR
            LIMIT_INPUT 1048576 LIMIT_COUNT 1 REGEX "__IMPORT_DESCRIPTOR_")
        if(NOT _JXL_IMPORT_DESCRIPTOR)
            set(_${_jxl_library}_IS_STATIC ON)
        endif()
    endif()
endforeach()

set(_JXL_STATIC_DEPENDENCY_VARIABLES )
if(_Jxl_LIBRARY_IS_STATIC)
    get_filename_component(_JXL_LIBRARY_DIR ${Jxl_LIBRARY} DIRECTORY)
    find_library(Jxl_CMS_LIBRARY NAMES jxl_cms jxl_cms-static
        HINTS ${_JXL_LIBRARY_DIR})
    find_library(Jxl_HWY_LIBRARY NAMES hwy
        HINTS ${_JXL_LIBRARY_DIR})
    find_library(Jxl_BROTLIENC_LIBRARY NAMES brotlienc brotlienc-static
        HINTS ${_JXL_LIBRARY_DIR})
    find_library(Jxl_BROTLIDEC_LIBRARY NAMES brotlidec brotlidec-static
        HINTS ${_JXL_LIBRARY_DIR})
    find_library(Jxl_BROTLICOMMON_LIBRARY NAMES brotlicommon brotlicommon-static
        HINTS ${_JXL_LIBRARY_DIR})
    mark_as_advanced(
        Jxl_CMS_LIBRARY
        Jxl_HWY_LIBRARY
        Jxl_BROTLIENC_LIBRARY
        Jxl_BROTLIDEC_LIBRARY
        Jxl_BROTLICOMMON_LIBRARY)
    set(_JXL_STATIC_DEPENDENCY_VARIABLES
        Jxl_HWY_LIBRARY
        Jxl_BROTLIENC_LIBRARY
        Jxl_BROTLIDEC_LIBRARY
        Jxl_BROTLICOMMON_LIBRARY)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jxl DEFAULT_MSG
    Jxl_LIBRARY
    Jxl_THREADS_LIBRARY
    Jxl_INCLUDE_DIR
    ${_JXL_STATIC_DEPENDENCY_VARIABLES})

mark_as_advanced(Jxl_LIBRARY Jxl_THREADS_LIBRARY Jxl_INCLUDE_DIR)

if(Jxl_FOUND AND NOT TARGET Jxl::Jxl)
    if(TARGET PkgConfig::PC_Jxl)
        add_library(Jxl::Jxl INTERFACE IMPORTED)
        set_target_properties(Jxl::Jxl PROPERTIES
            INTERFACE_LINK_LIBRARIES PkgConfig::PC_Jxl)
    else()
        add_library(Jxl::Jxl UNKNOWN IMPORTED)
        set_target_properties(Jxl::Jxl PROPERTIES
            IMPORTED_LOCATION ${Jxl_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${Jxl_INCLUDE_DIR})
    endif()

    if(_Jxl_LIBRARY_IS_STATIC)
        set(_JXL_STATIC_DEPENDENCIES )
        if(Jxl_CMS_LIBRARY)
            list(APPEND _JXL_STATIC_DEPENDENCIES ${Jxl_CMS_LIBRARY})
        endif()
        list(APPEND _JXL_STATIC_DEPENDENCIES
            ${Jxl_HWY_LIBRARY}
            ${Jxl_BROTLIENC_LIBRARY}
            ${Jxl_BROTLIDEC_LIBRARY}
            ${Jxl_BROTLICOMMON_LIBRARY})
        set_property(TARGET Jxl::Jxl APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES ${_JXL_STATIC_DEPENDENCIES})
        set_property(TARGET Jxl::Jxl APPEND PROPERTY
            INTERFACE_COMPILE_DEFINITIONS JXL_STATIC_DEFINE)
    endif()
endif()

if(Jxl_FOUND AND NOT TARGET Jxl::Threads)
    if(TARGET PkgConfig::PC_JxlThreads)
        add_library(Jxl::Threads INTERFACE IMPORTED)
        set_target_properties(Jxl::Threads PROPERTIES
            INTERFACE_LINK_LIBRARIES PkgConfig::PC_JxlThreads)
    else()
        add_library(Jxl::Threads UNKNOWN IMPORTED)
        set_target_properties(Jxl::Threads PROPERTIES
            IMPORTED_LOCATION ${Jxl_THREADS_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${Jxl_INCLUDE_DIR})
    endif()

    if(_Jxl_THREADS_LIBRARY_IS_STATIC)
        find_package(Threads REQUIRED)
        set_property(TARGET Jxl::Threads APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES Threads::Threads)
        set_property(TARGET Jxl::Threads APPEND PROPERTY
            INTERFACE_COMPILE_DEFINITIONS JXL_THREADS_STATIC_DEFINE)
    endif()
endif()
