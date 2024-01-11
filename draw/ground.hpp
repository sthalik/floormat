#pragma once

namespace floormat {

struct tile_shader;
struct chunk;

struct ground_mesh final
{
    ground_mesh();

    void draw(tile_shader& shader, chunk& c);
};

} // namespace floormat
