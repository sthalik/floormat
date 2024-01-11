#pragma once

namespace floormat {

struct tile_shader;
struct chunk;

struct wall_mesh
{
    wall_mesh();

    void draw(tile_shader& shader, chunk& c);

};

} // namespace floormat
