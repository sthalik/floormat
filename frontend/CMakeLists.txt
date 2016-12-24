project(floormat-src)

include_directories("${range-v3_INCLUDE_DIR}")

file(GLOB src *.h *.c *.hpp *.cpp)

add_executable(floormat ${src})
target_link_libraries(floormat ${MY_QT_LIBS})
install(TARGETS floormat DESTINATION .)
