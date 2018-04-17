project(floormat-backend)

include_directories(.)
file(GLOB src *.h *.c *.hpp *.cpp */*.h */*.c */*.hpp */*.cpp)

add_library(floormat-backend STATIC ${src})
