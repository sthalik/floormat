set(CMAKE_INSTALL_MESSAGE NEVER)
sets(STRING
     CMAKE_BUILD_TYPE Debug
     CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL
)
function(fm-userconfig-src)
    add_compile_options(-W4)
endfunction()

# Six -Otb3 vqsort compiles at once can exceed the runner's commit limit (C1002).
function(fm-userconfig-post)
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS floormat-hwy=3)
    set_property(TARGET floormat-hwy PROPERTY JOB_POOL_COMPILE floormat-hwy)
endfunction()
