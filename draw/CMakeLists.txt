set(self floormat-draw)
file(GLOB sources "*.cpp" CONFIGURE_ARGS)
add_library(${self} OBJECT "${sources}")
target_link_libraries(${self} PUBLIC Magnum::GL)
