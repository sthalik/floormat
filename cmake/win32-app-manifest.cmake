include_guard(GLOBAL)

if(WIN32)
    if(NOT MSVC)
        # A target of its own keeps the executables' compile definitions away from GNU windres.
        # It passes them unquoted to cmd.exe, which splits floormat-hwy's HWY_DISABLED_TARGETS at '|'.
        add_library(floormat-win32-app-manifest OBJECT "${CMAKE_CURRENT_LIST_DIR}/win32-app.rc")
        # Ninja gets no depfile from windres.
        set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/win32-app.rc" PROPERTIES
                                    OBJECT_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/win32-app.manifest")
    endif()

    function(fm_win32_app_manifest target)
        if(MSVC)
            target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/win32-app.manifest")
        else()
            # CMake embeds .manifest sources only with MSVC's linker.
            target_sources(${target} PRIVATE $<TARGET_OBJECTS:floormat-win32-app-manifest>)
        endif()
    endfunction()
else()
    function(fm_win32_app_manifest target)
    endfunction()
endif()
