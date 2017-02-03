project(floormat-backend)

file(GLOB src *.h *.c *.hpp *.cpp)

add_library(floormat-backend STATIC ${src})
