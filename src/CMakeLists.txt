add_library(floormat-common INTERFACE)
target_link_libraries(floormat-common INTERFACE swl-variant fmt::fmt)
target_include_directories(floormat-common SYSTEM INTERFACE ../external/gtl/include)
target_compile_definitions(floormat-common INTERFACE -DSWL_VARIANT_NO_CONSTEXPR_EMPLACE -DSWL_VARIANT_NO_STD_HASH)
get_target_property(variant-includes swl-variant INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(floormat-common SYSTEM INTERFACE ${variant-includes})
# -fprofile-use matches per function, so a use build that drops the driver has no entry for it.
# On floormat-common because it changes the layout of fm_settings.
if(FLOORMAT_NO_PGO_DRIVER)
    target_compile_definitions(floormat-common INTERFACE FLOORMAT_NO_PGO_DRIVER)
endif()

set(self floormat)
file(GLOB sources CONFIGURE_DEPENDS *.cpp ../shaders/*.cpp ../compat/*.cpp ../entity/*.cpp)
add_library(${self} OBJECT "${sources}")
target_link_libraries(
    ${self} PUBLIC
    floormat-common
    floormat-shader-res
    Magnum::GL
    Magnum::Magnum
    Magnum::Shaders
    #Magnum::DebugTools
)
target_link_libraries(${self} PRIVATE floormat-hwy)

#target_precompile_headers(${self} PUBLIC precomp.hpp)

if (WIN32 AND NOT MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set_property(SOURCE fps-counter.cpp APPEND PROPERTY COMPILE_OPTIONS "-mlong-double-80")
endif()

# Vendored musl C awaiting adaptation. Shrinks as the C-isms go; clang-cl reports
# CMAKE_CXX_COMPILER_ID as Clang, so testing MSVC would wrongly exclude it.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_property(SOURCE musl_getopt.cpp APPEND PROPERTY COMPILE_OPTIONS
        -Wno-sign-conversion -Wno-zero-as-null-pointer-constant -Wno-cast-qual
        -Wno-shorten-64-to-32 -Wno-conditional-uninitialized -Wno-comma -Wno-unused-macros)
endif()
