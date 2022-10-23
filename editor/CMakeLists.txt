set(self ${PROJECT_NAME}-editor)

corrade_add_resource(res ../resources.conf)

if(MSVC)
    set_property(SOURCE "${res}" APPEND PROPERTY COMPILE_OPTIONS "-W0")
else()
    set_property(SOURCE "${res}" APPEND PROPERTY COMPILE_OPTIONS "-w")
endif()

add_executable(${self} "${res}" "../loader/loader-impl.cpp")
target_link_libraries(${self} ${PROJECT_NAME}-main)

install(TARGETS ${self} RUNTIME DESTINATION bin)
