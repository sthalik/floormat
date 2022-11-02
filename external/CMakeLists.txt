project(floormat-external)

set(FLOORMAT_SUBMODULE-DEPENDENCIES ON CACHE BOOL
    "Use dependencies included in the source directory (needs git submodule update --init).")

foreach(_module corrade magnum magnum-integration magnum-plugins)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${_module}/modules")
endforeach()

if(MSVC)
    add_link_options(-WX:NO)
    set_property(DIRECTORY APPEND PROPERTY STATIC_LIBRARY_OPTIONS
        -IGNORE:4006 #warning LNK4006: main already defined in x.cpp.obj; second definition ignored
    )
    add_compile_options(
        -wd4244 # warning C4244: 'argument': conversion from 'int' to 'float', possible loss of data
        -wd4312 # warning C4312: 'reinterpret_cast': conversion from 'GLenum' to 'void *' of greater size
        -wd4251 # warning C4251: 't::f': class 'x' needs to have dll-interface to be used by clients of class 'y'
        -wd4456 # warning C4456: declaration of 'x' hides previous local declaration
        -wd4457 # #warning C4457: declaration of 'name' hides function parameter
        #-wd4530 # warning C4530: C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
        -wd4244 # warning C4244: 'initializing': conversion from 'unsigned int' to '_Ty', possible loss of data
        -wd4005 # warning C4005: '_USE_MATH_DEFINES': macro redefinition
    )
else()
    add_compile_options(
        -Wno-error
        -Wno-undef
    )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-old-style-cast")
endif()

if(FLOORMAT_SUBMODULE-DEPENDENCIES)
    add_definitions(
        -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS
        -DIMGUI_DISABLE_OBSOLETE_KEYIO
    )
    sets(STRING
         SDL_ASSERTIONS                         release
         SDL_INSTALL_CMAKEDIR                   "share/cmake/SDL2"
         NLOHMANN_JSON_CONFIG_INSTALL_DIR       "share/cmake/nlohmann_json"
    )
    sets(BOOL
         JSON_Diagnostics        ON
         JSON_Install            ON
         JSON_MultipleHeaders    ON
         JSON_SystemInclude      ON
    )
    sets(BOOL
         CORRADE_BUILD_STATIC                   OFF
         CORRADE_BUILD_TESTS                    OFF
         CORRADE_BUILD_DEPRECATED               OFF
         CORRADE_MSVC_COMPATIBILITY             OFF
         CORRADE_UTILITY_USE_ANSI_COLORS        ON
         CORRADE_WITH_TESTSUITE                 ON

         MAGNUM_BUILD_STATIC                    OFF
         MAGNUM_BUILD_TESTS                     OFF
         MAGNUM_BUILD_DEPRECATED                ON

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
         MAGNUM_WITH_PNGIMPORTER                OFF
         MAGNUM_WITH_SDL2APPLICATION            ON
         MAGNUM_WITH_SHADERCONVERTER            ON
         MAGNUM_WITH_SHADERS                    ON
         MAGNUM_WITH_SHADERTOOLS                ON
         MAGNUM_WITH_TGAIMAGECONVERTER          ON
         MAGNUM_WITH_TGAIMPORTER                ON
         MAGNUM_WITH_WEBPIMPORTER               OFF
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
         SDL2_DISABLE_INSTALL                   OFF
         SDL2_DISABLE_SDL2MAIN                  ON

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
    )
    if(NOT WIN32)
        set(MAGNUM_WITH_WINDOWLESSWGLAPPLICATION OFF)
    endif()
endif()

fm_run_hook(fm-userconfig-external-pre)

if(FLOORMAT_SUBMODULE-DEPENDENCIES)
    sets(PATH IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
    add_subdirectory(json)
    add_subdirectory(sdl2)
    add_subdirectory(corrade)
    add_subdirectory(magnum)
    add_subdirectory(magnum-plugins)
    add_subdirectory(magnum-integration)
endif()

if(FLOORMAT_SUBMODULE-DEPENDENCIES)
    if(MSVC)
        target_compile_options(CorradeTestSuiteObjects PRIVATE -EHsc)
    endif()
endif()

fm_run_hook(fm-userconfig-external)

find_package(SDL2 QUIET REQUIRED)
find_package(Corrade QUIET REQUIRED)
find_package(ImGui QUIET REQUIRED)
find_package(Magnum QUIET REQUIRED)
find_package(MagnumPlugins QUIET REQUIRED)
find_package(MagnumIntegration QUIET REQUIRED ImGui)

fm_run_hook(fm-userconfig-external-post)
