project(floormat-text)

include_directories("../../backend")

file(GLOB src *.h *.c *.hpp *.cpp)

add_executable(floormat-text ${src})
target_link_libraries(floormat-text ${MY_QT_LIBS} floormat-backend)
install(TARGETS floormat-text DESTINATION .)
