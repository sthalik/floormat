#pragma once

namespace floormat {

struct tile_shader;
struct chunk;

struct floor_mesh final
{
    floor_mesh();

    void draw(tile_shader& shader, chunk& c);
};

} // namespace floormat
