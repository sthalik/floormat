#pragma once

namespace floormat {

struct tile_shader;
class chunk;

struct ground_mesh
{
    ground_mesh();

    void draw(tile_shader& shader, chunk& c);
};

} // namespace floormat
