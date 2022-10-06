set(self ${PROJECT_NAME})
file(GLOB sources "*.cpp" "../shaders/*.cpp" "../serialize/*.cpp" CONFIGURE_ARGS)
add_library(${self} STATIC "${sources}")
target_link_libraries(
        ${self} PUBLIC
        Magnum::GL
        Magnum::Magnum
        Magnum::Shaders
        nlohmann_json::nlohmann_json
)
target_include_directories(${self} PRIVATE ${GLM_INCLUDE_DIRS})
target_include_directories(${self} PRIVATE magnum-integration/src)
