include(GetGitRevisionDescription)

find_package(Git QUIET)
if(GIT_FOUND)
    git_describe(FLOORMAT_COMMIT --tags --always --dirty)
endif()

unset(_build_type)
if(CMAKE_BUILD_TYPE)
    string(TOUPPER ${CMAKE_BUILD_TYPE} _build_type)
    if (NOT _build_type STREQUAL "DEBUG")
        unset(_build_type)
    else()
        set(_build_type "-${_build_type}")
    endif()
endif()

file(WRITE ${CMAKE_BINARY_DIR}/floormat-version.h "#define FLOORMAT_VERSION \"${FLOORMAT_COMMIT}${_build_type}\"")

set(version-string "
#include \"compat/export.hpp\"

#ifdef __cplusplus
extern \"C\"
#else
extern
#endif
FLOORMAT_EXPORT
const char* floormat_version;

const char* floormat_version = \"${OPENTRACK_COMMIT}${_build_type}\";
")

set(crapola-ver)
if(EXISTS ${CMAKE_BINARY_DIR}/version.c)
    file(READ ${CMAKE_BINARY_DIR}/version.c crapola-ver)
endif()

if(NOT (crapola-ver STREQUAL version-string))
    file(WRITE ${CMAKE_BINARY_DIR}/version.c "${version-string}")
endif()

add_library(floormat-version STATIC ${CMAKE_BINARY_DIR}/version.c)
if(NOT MSVC)
    set_property(TARGET floormat-version APPEND_STRING PROPERTY COMPILE_FLAGS "-fno-lto")
endif()

