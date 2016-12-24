project(floormat-backend)

include_directories("${range-v3_INCLUDE_DIR}")

file(GLOB src *.h *.c *.hpp *.cpp)

add_library(floormat-backend STATIC ${src})
