set(self ${PROJECT_NAME}-editor)

file(GLOB sources "*.cpp" "../loader/*.cpp" CONFIGURE_ARGS)
corrade_add_resource(res "../resources.conf")

if(MSVC)
    set_property(SOURCE "${res}" APPEND PROPERTY COMPILE_OPTIONS "-W0")
else()
    set_property(SOURCE "${res}" APPEND PROPERTY COMPILE_OPTIONS "-w")
endif()

link_libraries(MagnumIntegration::ImGui fmt::fmt)

add_executable(${self} "${sources}" "${res}")
target_link_libraries(${self} ${PROJECT_NAME}-main)

if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()

install(TARGETS ${self} RUNTIME DESTINATION bin)
