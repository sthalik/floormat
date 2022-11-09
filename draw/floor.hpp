#pragma once
#include "tile-defs.hpp"
#include <array>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>

namespace floormat {

struct tile_ref;
struct tile_shader;
struct chunk;

struct floor_mesh final
{
    floor_mesh();
    floor_mesh(floor_mesh&&) = delete;
    floor_mesh(const floor_mesh&) = delete;

    void draw(tile_shader& shader, chunk& c);
};

} // namespace floormat
