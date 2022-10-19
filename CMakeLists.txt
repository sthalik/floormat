list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(game)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

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

set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS OFF)
set_directory_properties(PROPERTIES CORRADE_CXX_STANDARD 20)
set_directory_properties(PROPERTIES INTERFACE_CORRADE_CXX_STANDARD 20)

foreach(_module corrade magnum magnum-integration magnum-plugins)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${_module}/modules")
endforeach()

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

sets(PATH ImGui_DIR "${CMAKE_SOURCE_DIR}/imgui")
sets(PATH ImGui_INCLUDE_DIR "${ImGui_DIR}")

if(MSVC)
    add_compile_options(/permissive-)
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

sets(BOOL
     CORRADE_BUILD_STATIC                   OFF
     CORRADE_BUILD_TESTS                    OFF
     CORRADE_BUILD_DEPRECATED               OFF

     CORRADE_MSVC_COMPATIBILITY             OFF

     CORRADE_UTILITY_USE_ANSI_COLORS        ON
     CORRADE_WITH_TESTSUITE                 ON

     MAGNUM_BUILD_STATIC                    OFF
     MAGNUM_BUILD_TESTS                     OFF

     MAGNUM_WITH_ANYIMAGECONVERTER          ON
     MAGNUM_WITH_ANYIMAGEIMPORTER           ON
     MAGNUM_WITH_DISTANCEFIELDCONVERTER     ON
     MAGNUM_WITH_FONTCONVERTER              ON
     MAGNUM_WITH_GL_INFO                    ON
     MAGNUM_WITH_IMAGECONVERTER             ON
     MAGNUM_WITH_MAGNUMFONT                 ON
     MAGNUM_WITH_MAGNUMFONTCONVERTER        ON
     MAGNUM_WITH_OBJIMPORTER                ON
     MAGNUM_WITH_OPENGLTESTER               ON
     MAGNUM_WITH_SDL2APPLICATION            ON
     MAGNUM_WITH_SHADERCONVERTER            ON
     MAGNUM_WITH_SHADERS                    ON
     MAGNUM_WITH_SHADERTOOLS                ON
     MAGNUM_WITH_TGAIMAGECONVERTER          ON
     MAGNUM_WITH_TGAIMPORTER                ON
     MAGNUM_WITH_WINDOWLESSWGLAPPLICATION   ON

     MAGNUM_WITH_DDSIMPORTER                ON
     MAGNUM_WITH_GLTFIMPORTER               ON
     MAGNUM_WITH_ICOIMPORTER                ON
     MAGNUM_WITH_PRIMITIVEIMPORTER          ON
     MAGNUM_WITH_STBIMAGECONVERTER          ON
     MAGNUM_WITH_STBIMAGEIMPORTER           ON
     MAGNUM_WITH_STBTRUETYPEFONT            ON
     MAGNUM_WITH_TINYGLTFIMPORTER           OFF

     MAGNUM_WITH_BULLET                     OFF
     MAGNUM_WITH_DART                       OFF
     MAGNUM_WITH_EIGEN                      OFF
     MAGNUM_WITH_GLM                        OFF
     MAGNUM_WITH_IMGUI                      ON
)

sets(BOOL
     SDL_ASSERTIONS                         release

     SDL_3DNOW                              OFF
     SDL_ALTIVEC                            OFF
     #SDL_ASSEMBLY                          OFF
     SDL_ATOMIC                             OFF
     SDL_AUDIO                              OFF
     SDL_CPUINFO                            OFF
     SDL_DIRECTX                            OFF
     SDL_DISKAUDIO                          OFF
     SDL_DUMMYAUDIO                         OFF
     SDL_DUMMYVIDEO                         OFF
     SDL_FILE                               OFF
     SDL_FILESYSTEM                         OFF
     SDL_HAPTIC                             OFF
     SDL_HIDAPI                             OFF
     SDL_HIDAPI_JOYSTICK                    OFF
     SDL_JOYSTICK                           OFF
     SDL_LIBC                               ON
     SDL_LOADSO                             ON
     SDL_LOCALE                             OFF
     SDL_MISC                               OFF
     #SDL_MMX                                OFF
     SDL_OPENGL                             ON
     SDL_OPENGLES                           OFF
     SDL_POWER                              OFF
     SDL_RENDER                             OFF
     SDL_RENDER_D3D                         OFF
     SDL_SENSOR                             OFF
     #SDL_SSE                               OFF
     #SDL_SSE2                              OFF
     #SDL_SSE3                              OFF
     #SDL_SSEMATH                           OFF
     SDL_SYSTEM_ICONV                       OFF
     SDL_TEST                               OFF
     SDL_THREADS                            ON
     SDL_TIMERS                             OFF
     SDL_VIDEO                              ON
     SDL_VIRTUAL_JOYSTICK                   OFF
     SDL_VULKAN                             OFF
     SDL_WASAPI                             OFF
     SDL_XINPUT                             OFF

     SDL2_DISABLE_INSTALL                   OFF
     SDL2_DISABLE_SDL2MAIN                  ON
)

sets(BOOL
     JSON_Diagnostics        ON
     JSON_Install            ON
     JSON_MultipleHeaders    ON
     JSON_SystemInclude      ON
)

set(_userconfig "userconfig-${CMAKE_CXX_COMPILER_ID}.cmake")
if(EXISTS "${PROJECT_SOURCE_DIR}/${_userconfig}")
    include("${PROJECT_SOURCE_DIR}/${_userconfig}" NO_POLICY_SCOPE)
else()
    message(STATUS "user config '${_userconfig}' not found")
endif()

find_package(ImGui QUIET REQUIRED)

add_subdirectory(json)

add_subdirectory(sdl2)
find_package(SDL2 QUIET REQUIRED)

add_subdirectory(corrade)
find_package(Corrade QUIET REQUIRED)

add_subdirectory(magnum)
find_package(Magnum QUIET REQUIRED)

add_subdirectory(magnum-plugins)
find_package(MagnumPlugins QUIET REQUIRED)

add_subdirectory(magnum-integration)
find_package(MagnumIntegration QUIET REQUIRED ImGui)

include_directories(.)
include_directories(src)

if(MSVC)
    add_definitions(/FI"compat/prelude.hpp")
else()
    add_definitions(-include compat/prelude.hpp)
endif()

if(MSVC)
    target_compile_options(CorradeTestSuite PUBLIC -EHsc)
    add_compile_options(-W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-old-style-cast -Wno-padded -Wno-ignored-attributes)
    add_compile_options(-fcolor-diagnostics)
    add_compile_options(-ferror-limit=5)
endif()

add_subdirectory(src)
add_subdirectory(draw)
add_subdirectory(main)
add_subdirectory(anim-crop-tool)
add_subdirectory(test)

install(DIRECTORY images DESTINATION "share/${PROJECT_NAME}")
