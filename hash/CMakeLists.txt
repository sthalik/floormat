set(self floormat-hash)
file(GLOB sources CONFIGURE_DEPENDS *.cpp)
add_library(${self} OBJECT "${sources}")
target_link_libraries(
    ${self} PUBLIC
    Corrade::Containers
    #Corrade::Utility
    Magnum::Magnum
)

#target_precompile_headers(${self} PUBLIC precomp.hpp)
