#pragma once

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
