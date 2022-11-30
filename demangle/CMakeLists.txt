cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(demumble CXX)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_DEFAULT 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

if (NOT MSVC)
    add_compile_options(-Wall -fno-exceptions -fno-rtti)
    # 10.9 chosen somewhat arbitrary; it's the first target where clang defaults
    # to libc++ and ld64 defaults to stripping __TEXT,__eh_frame.
    if (APPLE)
        add_compile_options(-mmacosx-version-min=10.9)
    else()
        add_compile_options(-fno-plt)
    endif()
    add_compile_options(-Wno-error -Wno-sign-conversion -Wno-unused -Wno-unused-parameter)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang$")
        add_compile_options(-Wno-shorten-64-to-32 -Wno-implicit-int-conversion -Wno-class-varargs -Wno-extra-semi-stmt)
    endif()
else()
    add_compile_options(-Zc:inline -permissive- -GR-)
    add_compile_options(-wd4100 -wd4244 -wd4267)
    add_definitions(-D_HAS_EXCEPTIONS=0 -DNOMINMAX -D_USE_MATH_DEFINES=1)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)  # The LLVM build sets this.
endif()

include_directories(SYSTEM third_party/llvm/include)
add_library(demumble STATIC
    third_party/llvm/lib/Demangle/Demangle.cpp
    third_party/llvm/lib/Demangle/ItaniumDemangle.cpp
    third_party/llvm/lib/Demangle/MicrosoftDemangle.cpp
    third_party/llvm/lib/Demangle/MicrosoftDemangleNodes.cpp
    demangle.cpp)
target_link_libraries(demumble PUBLIC Corrade::Containers)
