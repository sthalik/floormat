set(self floormat-editor)

file(GLOB sources "*.cpp" CONFIGURE_ARGS)
corrade_add_resource(res "../resources.conf")

if(MSVC)
    set_property(SOURCE "${res}" APPEND PROPERTY COMPILE_OPTIONS "-W0")
else()
    set_property(SOURCE "${res}" APPEND PROPERTY COMPILE_OPTIONS "-w")
endif()

link_libraries(MagnumIntegration::ImGui fmt::fmt)
link_libraries(floormat-main floormat-loader floormat-serialize)

add_executable(${self} "${sources}" "${res}")
if(WIN32)
    target_sources(${self} PRIVATE "../main/floormat.rc")
endif()

if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${self} PRIVATE precomp.hpp)
endif()

install(TARGETS ${self} RUNTIME DESTINATION bin)
