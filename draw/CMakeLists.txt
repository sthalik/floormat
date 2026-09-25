set(self floormat-draw)
file(GLOB sources CONFIGURE_DEPENDS "*.cpp")
add_library(${self} OBJECT "${sources}")
target_link_libraries(${self} PUBLIC Magnum::GL MagnumIntegration::ImGui)
