project(floormat-text)

include_directories("${range-v3_INCLUDE_DIR}")

file(GLOB src *.h *.c *.hpp *.cpp)

add_executable(floormat-text ${src})
target_link_libraries(floormat-text ${MY_QT_LIBS})
install(TARGETS floormat-text DESTINATION .)
