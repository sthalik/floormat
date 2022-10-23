file(GLOB sources "*.cpp" CONFIGURE_ARGS)

link_libraries(${PROJECT_NAME})
link_libraries(Magnum::Sdl2Application Corrade::Containers Magnum::GL Magnum::Trade)
link_libraries(MagnumIntegration::ImGui)

add_library(${PROJECT_NAME}-main STATIC "${sources}")

if(FLOORMAT_PRECOMPILED-HEADERS)
    target_precompile_headers(${PROJECT_NAME}-main PRIVATE precomp.hpp)
endif()
