set(CMAKE_INSTALL_MESSAGE NEVER)
sets(STRING
     CMAKE_BUILD_TYPE DEBUG
     CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL
)
function(fm-userconfig-src)
    add_compile_options(-W4)
endfunction()
