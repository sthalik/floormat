set(self floormat-editor)

file(GLOB sources "*.cpp" "tests/*.cpp" CONFIGURE_ARGS)
corrade_add_resource(res "../resources.conf")

set_property(SOURCE "${res}" APPEND PROPERTY COMPILE_OPTIONS "-w") # no warnings

add_library(${self}_o OBJECT "${res}" "${sources}")
target_link_libraries(${self}_o PUBLIC MagnumIntegration::ImGui fmt::fmt nlohmann_json::nlohmann_json tsl::robin_map)

if(WIN32)
    target_sources(${self}_o PRIVATE "../main/floormat.rc")
endif()

if(NOT SDL2_INCLUDE_DIRS)
    if(NOT TARGET SDL2::SDL2)
        find_package(SDL2 QUIET REQUIRED)
    else()
        get_target_property(SDL2_INCLUDE_DIRS SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
    endif()
endif()
set_property(SOURCE "events.cpp" "tests.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}")

add_executable(${self} dummy.cc)
if (FLOORMAT_ASAN)
    set_target_properties(${self} PROPERTIES OUTPUT_NAME "floormat-editor-asan")
endif()
target_link_libraries(${self} PRIVATE ${self}_o floormat-main floormat-serialize floormat-draw floormat)

fm_install_executable(${self})
