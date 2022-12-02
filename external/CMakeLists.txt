project(floormat-external)

set(FLOORMAT_SUBMODULE-DEPENDENCIES ON CACHE BOOL
    "Use dependencies included in the source directory (needs git submodule update --init).")
set(FLOORMAT_SUBMODULE-SDL2 ON CACHE BOOL "SDL2 as submodule")

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
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-old-style-cast")
endif()

if(FLOORMAT_SUBMODULE-DEPENDENCIES)
    add_definitions(
        -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS
        -DIMGUI_DISABLE_OBSOLETE_KEYIO
        -DJSON_HAS_THREE_WAY_COMPARISON=1
    )
    sets(STRING
         SDL_ASSERTIONS                         release
         SDL_INSTALL_CMAKEDIR                   "share/cmake/SDL2"
         NLOHMANN_JSON_CONFIG_INSTALL_DIR       "share/cmake/nlohmann_json"
    )
    sets(BOOL

         FMT_SYSTEM_HEADERS                                 ON

         JSON_Diagnostics                                   ON
         JSON_Install                                       ON
         JSON_MultipleHeaders                               ON
         JSON_SystemInclude                                 ON

         CORRADE_BUILD_STATIC                               OFF
         CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT    OFF
         MAGNUM_BUILD_STATIC                                OFF
         MAGNUM_BUILD_PLUGINS_STATIC                        OFF

         CORRADE_BUILD_STATIC_UNIQUE_GLOBALS                OFF
         CORRADE_BUILD_TESTS                                ON
         CORRADE_BUILD_DEPRECATED                           OFF
         CORRADE_MSVC_COMPATIBILITY                         OFF
         CORRADE_UTILITY_USE_ANSI_COLORS                    ON
         CORRADE_WITH_TESTSUITE                             ON
         MAGNUM_BUILD_TESTS                                 ON
         MAGNUM_BUILD_DEPRECATED                            OFF

         MAGNUM_WITH_SDL2APPLICATION                        ON
         MAGNUM_WITH_WINDOWLESSWGLAPPLICATION               OFF
         MAGNUM_WITH_WINDOWLESSGLXAPPLICATION               OFF
         MAGNUM_WITH_WINDOWLESSCGLAPPLICATION               OFF

         MAGNUM_WITH_ANYAUDIOIMPORTER                       OFF
         MAGNUM_WITH_ANYIMAGEIMPORTER                       OFF
         MAGNUM_WITH_ANYSCENEIMPORTER                       OFF
         MAGNUM_WITH_ASSIMPIMPORTER                         OFF
         MAGNUM_WITH_ASTCIMPORTER                           OFF
         MAGNUM_WITH_BASISIMPORTER                          OFF
         MAGNUM_WITH_CGLTFIMPORTER                          OFF
         MAGNUM_WITH_DEVILIMAGEIMPORTER                     OFF
         MAGNUM_WITH_DRFLACAUDIOIMPORTER                    OFF
         MAGNUM_WITH_DRMP3AUDIOIMPORTER                     OFF
         MAGNUM_WITH_DRWAVAUDIOIMPORTER                     OFF
         MAGNUM_WITH_FAAD2AUDIOIMPORTER                     OFF
         MAGNUM_WITH_GLTFIMPORTER                           OFF
         MAGNUM_WITH_ICOIMPORTER                            OFF
         MAGNUM_WITH_JPEGIMPORTER                           OFF
         MAGNUM_WITH_KTXIMPORTER                            OFF
         MAGNUM_WITH_OBJIMPORTER                            OFF
         MAGNUM_WITH_OPENEXRIMPORTER                        OFF
         MAGNUM_WITH_OPENGEXIMPORTER                        OFF
         MAGNUM_WITH_PNGIMPORTER                            OFF
         MAGNUM_WITH_PRIMITIVEIMPORTER                      OFF
         MAGNUM_WITH_STANFORDIMPORTER                       OFF
         MAGNUM_WITH_STBVORBISAUDIOIMPORTER                 OFF
         MAGNUM_WITH_STLIMPORTER                            OFF
         MAGNUM_WITH_TINYGLTFIMPORTER                       OFF
         MAGNUM_WITH_WAVAUDIOIMPORTER                       OFF
         MAGNUM_WITH_WEBPIMPORTER                           OFF

         MAGNUM_WITH_DDSIMPORTER                            ON
         MAGNUM_WITH_STBIMAGEIMPORTER                       ON
         MAGNUM_WITH_STBTRUETYPEFONT                        ON
         MAGNUM_WITH_TGAIMPORTER                            ON

         MAGNUM_WITH_GL_INFO                                ON
         MAGNUM_WITH_MAGNUMFONT                             ON
         MAGNUM_WITH_OPENGLTESTER                           ON
         MAGNUM_WITH_SHADERS                                ON
         MAGNUM_WITH_SHADERTOOLS                            ON

         MAGNUM_WITH_ANYIMAGECONVERTER                      OFF
         MAGNUM_WITH_ANYSCENECONVERTER                      OFF
         MAGNUM_WITH_ANYSHADERCONVERTER                     OFF
         MAGNUM_WITH_BASISIMAGECONVERTER                    OFF
         MAGNUM_WITH_DISTANCEFIELDCONVERTER                 OFF
         MAGNUM_WITH_GLSLANGSHADERCONVERTER                 OFF
         MAGNUM_WITH_GLTFSCENECONVERTER                     OFF
         MAGNUM_WITH_IMAGECONVERTER                         OFF
         MAGNUM_WITH_JPEGIMAGECONVERTER                     OFF
         MAGNUM_WITH_KTXIMAGECONVERTER                      OFF
         MAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER            OFF
         MAGNUM_WITH_MINIEXRIMAGECONVERTER                  OFF
         MAGNUM_WITH_OPENEXRIMAGECONVERTER                  OFF
         MAGNUM_WITH_PNGIMAGECONVERTER                      OFF
         MAGNUM_WITH_SCENECONVERTER                         OFF
         MAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER              OFF
         MAGNUM_WITH_STANFORDSCENECONVERTER                 OFF
         MAGNUM_WITH_STBRESIZEIMAGECONVERTER                ON
         MAGNUM_WITH_TGAIMAGECONVERTER                      OFF

         MAGNUM_WITH_MAGNUMFONTCONVERTER                    ON
         MAGNUM_WITH_SHADERCONVERTER                        ON
         MAGNUM_WITH_STBDXTIMAGECONVERTER                   ON
         MAGNUM_WITH_STBIMAGECONVERTER                      ON

         MAGNUM_WITH_BULLET                                 OFF
         MAGNUM_WITH_DART                                   OFF
         MAGNUM_WITH_EIGEN                                  OFF
         MAGNUM_WITH_GLM                                    OFF
         MAGNUM_WITH_IMGUI                                  ON
    )
    sets(BOOL
         SDL2_DISABLE_INSTALL                               OFF
         SDL2_DISABLE_SDL2MAIN                              ON

         SDL_3DNOW                                          OFF
         SDL_ALTIVEC                                        OFF
         #SDL_ASSEMBLY                                      OFF
         SDL_ATOMIC                                         OFF
         SDL_AUDIO                                          OFF
         SDL_CPUINFO                                        OFF
         SDL_DIRECTX                                        OFF
         SDL_DISKAUDIO                                      OFF
         SDL_DUMMYAUDIO                                     OFF
         SDL_DUMMYVIDEO                                     OFF
         SDL_FILE                                           OFF
         SDL_FILESYSTEM                                     OFF
         SDL_HAPTIC                                         OFF
         SDL_HIDAPI                                         OFF
         SDL_HIDAPI_JOYSTICK                                OFF
         SDL_JOYSTICK                                       OFF
         SDL_LIBC                                           OFF
         SDL_LOADSO                                         ON
         SDL_LOCALE                                         OFF
         SDL_MISC                                           OFF
         #SDL_MMX                                            OFF
         SDL_OPENGL                                         ON
         SDL_OPENGLES                                       OFF
         SDL_POWER                                          OFF
         SDL_RENDER                                         OFF
         SDL_RENDER_D3D                                     OFF
         SDL_SENSOR                                         OFF
         #SDL_SSE                                           OFF
         #SDL_SSE2                                          OFF
         #SDL_SSE3                                          OFF
         #SDL_SSEMATH                                       OFF
         SDL_SYSTEM_ICONV                                   OFF
         SDL_TEST                                           OFF
         SDL_THREADS                                        ON
         SDL_TIMERS                                         OFF
         SDL_VIDEO                                          ON
         SDL_VIRTUAL_JOYSTICK                               OFF
         SDL_VULKAN                                         OFF
         SDL_WASAPI                                         OFF
         SDL_XINPUT                                         OFF
    )
    if(WIN32)
        sets(BOOL MAGNUM_WITH_WINDOWLESSWGLAPPLICATION ON)
    elseif(APPLE)
        sets(BOOL MAGNUM_WITH_WINDOWLESSCGLAPPLICATION ON)
    else()
        sets(BOOL MAGNUM_WITH_WINDOWLESSGLXAPPLICATION ON)
    endif()
endif()

set(fm-quiet-message 0)

function(message m)
    list(REMOVE_AT ARGV 0)
    if(fm-quiet-message)
        if(m STREQUAL FATAL_ERROR OR m STREQUAL SEND_ERROR OR m STREQUAL WARNING OR m STREQUAL AUTHOR_WARNING)
            _message("${m}" "${ARGV}")
        endif()
    else()
        _message("${m}" "${ARGV}")
    endif()
endfunction()

if(FLOORMAT_SUBMODULE-DEPENDENCIES)
    sets(PATH IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")

    function(fm_add_sdl2)
        if(FLOORMAT_SUBMODULE-SDL2)
            unset(CMAKE_C_VISIBILITY_PRESET)
            unset(CMAKE_VISIBILITY_INLINES_HIDDEN)
            add_subdirectory(sdl2)
        endif()
    endfunction()

    function(fm_add_luajit)
        set(CMAKE_C_STANDARD 11)
        set(CMAKE_C_EXTENSIONS 1)
        unset(CMAKE_C_VISIBILITY_PRESET)
        unset(CMAKE_VISIBILITY_INLINES_HIDDEN)
        if(CMAKE_COMPILER_IS_GNUCXX)
            # HACK
            add_compile_options(-fno-sanitize=all)
            add_link_options(-fno-sanitize=all)
            add_subdirectory(luajit)
        endif()
    endfunction()

    fm_run_hook(fm-userconfig-external)

    function(fm_add_libs)
        if(fm-external-configured)
            set(fm-quiet-message 1)
        endif()
        if(MAGNUM_BUILD_TESTS OR CORRADE_BUILD_TESTS AND NOT DEFINED CORRADE_TESTSUITE_TEST_TARGET)
            sets(STRING CORRADE_TESTSUITE_TEST_TARGET corrade-test)
        endif()
        fm_add_sdl2()
        #fm_add_luajit()
        if(MSVC)
            add_compile_options(-GR-)
        else()
            add_compile_options(-fno-rtti)
        endif()
        add_subdirectory(fmt)
        add_subdirectory(json)
        add_subdirectory(corrade)
        if(MSVC)
            target_compile_options(CorradeTestSuiteObjects PUBLIC -GR)
            target_compile_options(CorradeTestSuite PUBLIC -GR)
        else()
            target_compile_options(CorradeTestSuiteObjects PUBLIC -frtti)
            target_compile_options(CorradeTestSuite PUBLIC -frtti)
        endif()
        add_subdirectory(magnum)
        add_subdirectory(magnum-plugins)
        add_subdirectory(magnum-integration)
    endfunction()

    if(MAGNUM_BUILD_PLUGINS_STATIC)
        sets(BOOL
             MAGNUM_WITH_SHADERTOOLS OFF
             MAGNUM_WITH_SHADERCONVERTER OFF
        )
    endif()
    fm_add_libs()
endif()

if(FLOORMAT_SUBMODULE-DEPENDENCIES)
    if(MSVC)
        target_compile_options(CorradeTestSuiteObjects PRIVATE -EHsc)
    endif()
    fm_run_hook(fm-userconfig-post)
endif()

if(fm-external-configured)
    set(fm-quiet-message 1)
endif()
find_package(SDL2 QUIET REQUIRED)
find_package(Corrade QUIET REQUIRED)
find_package(ImGui QUIET REQUIRED)
find_package(Magnum QUIET REQUIRED)
find_package(MagnumPlugins QUIET REQUIRED)
find_package(MagnumIntegration QUIET REQUIRED ImGui)

if(NOT FLOORMAT_SUBMODULE-DEPENDENCIES)
    find_package(fmt QUIET REQUIRED)
endif()

set(fm-quiet-message 0)
sets(INTERNAL fm-external-configured 1)
