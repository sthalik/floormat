add_library(floormat-random OBJECT random.cpp gaussian.cpp os-seed.cpp chacha.cpp)
target_link_libraries(floormat-random PUBLIC Magnum::Magnum PRIVATE floormat-hwy)
if(WIN32)
    target_link_libraries(floormat-random PRIVATE bcrypt)
endif()
fm_no_fast_math(PRECISE SOURCE gaussian.cpp)

set(self floormat-random-period)

add_executable(${self} period-test.cpp ../compat/assert.cpp)
target_link_libraries(${self} PRIVATE floormat-random floormat-common Magnum::Magnum)

fm_win32_app_manifest(${self})
fm_install_executable(${self})

set(self floormat-random-stream)

add_executable(${self} stream.cpp ../compat/assert.cpp)
target_link_libraries(${self} PRIVATE floormat-random floormat-common Magnum::Magnum)

fm_win32_app_manifest(${self})
fm_install_executable(${self})
