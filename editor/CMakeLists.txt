set(self floormat-editor)

file(GLOB sources "*.cpp" CONFIGURE_ARGS)
corrade_add_resource(res "../resources.conf")

if(MSVC)
    set_property(SOURCE "${res}" APPEND PROPERTY COMPILE_OPTIONS "-W0")
else()
    set_property(SOURCE "${res}" APPEND PROPERTY COMPILE_OPTIONS "-w")
endif()

add_executable(${self} "${res}" "${sources}")
target_link_libraries(${self} PRIVATE MagnumIntegration::ImGui fmt::fmt nlohmann_json::nlohmann_json)

if(NOT SDL2_INCLUDE_DIRS)
    if(NOT TARGET SDL2::SDL2)
        find_package(SDL2 QUIET REQUIRED)
    else()
        get_target_property(SDL2_INCLUDE_DIRS SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
    endif()
endif()
target_include_directories(${self} SYSTEM PRIVATE "${SDL2_INCLUDE_DIRS}")

target_link_libraries(${self} PRIVATE floormat-main floormat-serialize floormat-draw floormat)

if(WIN32)
    target_sources(${self} PRIVATE "../main/floormat.rc")
endif()

if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()

install(TARGETS ${self} RUNTIME DESTINATION bin)
