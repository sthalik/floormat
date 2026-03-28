set(self floormat)
file(GLOB sources *.cpp ../shaders/*.cpp ../compat/*.cpp ../entity/*.cpp CONFIGURE_ARGS)
add_library(${self} OBJECT "${sources}")
target_link_libraries(
    ${self} PUBLIC
    floormat-shader-res
    Magnum::GL
    Magnum::Magnum
    Magnum::Shaders
    #Magnum::DebugTools
    fmt::fmt
    tsl::robin_map
)

#target_precompile_headers(${self} PUBLIC precomp.hpp)

if (WIN32 AND NOT MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set_property(SOURCE fps-counter.cpp APPEND PROPERTY COMPILE_OPTIONS "-mlong-double-80")
endif()
