#include "chunk.hpp"
#include "src/tile-atlas.hpp"
#include "compat/LooseQuadtree-impl.h"

namespace floormat {

bool chunk::empty(bool force) const noexcept
{
    if (!force && !_maybe_empty)
        return false;
    for (std::size_t i = 0; i < TILE_COUNT; i++)
        if (_ground_atlases[i] || _wall_atlases[i*2 + 0] || _wall_atlases[i*2 + 1] || _scenery_atlases[i])
            return _maybe_empty = false;
    return true;
}

tile_atlas* chunk::ground_atlas_at(std::size_t i) const noexcept { return _ground_atlases[i].get(); }
tile_atlas* chunk::wall_atlas_at(std::size_t i) const noexcept { return _wall_atlases[i].get(); }
anim_atlas* chunk::scenery_atlas_at(std::size_t i) const noexcept { return _scenery_atlases[i].get(); }

tile_ref chunk::operator[](std::size_t idx) noexcept { return { *this, std::uint8_t(idx) }; }
tile_proto chunk::operator[](std::size_t idx) const noexcept { return tile_proto(tile_ref { *const_cast<chunk*>(this), std::uint8_t(idx) }); }
tile_ref chunk::operator[](local_coords xy) noexcept { return operator[](xy.to_index()); }
tile_proto chunk::operator[](local_coords xy) const noexcept { return operator[](xy.to_index()); }

auto chunk::begin() noexcept -> iterator { return iterator { *this, 0 }; }
auto chunk::end() noexcept -> iterator { return iterator { *this, TILE_COUNT }; }
auto chunk::cbegin() const noexcept -> const_iterator { return const_iterator { *this, 0 }; }
auto chunk::cend() const noexcept -> const_iterator { return const_iterator { *this, TILE_COUNT }; }
auto chunk::begin() const noexcept -> const_iterator { return cbegin(); }
auto chunk::end() const noexcept -> const_iterator { return cend(); }

void chunk::mark_ground_modified() noexcept { _ground_modified = true; _pass_modified = true; }
void chunk::mark_walls_modified() noexcept { _walls_modified = true; _pass_modified = true; }
void chunk::mark_scenery_modified() noexcept { _scenery_modified = true; _pass_modified = true; }

void chunk::mark_modified() noexcept
{
    mark_ground_modified();
    mark_walls_modified();
    mark_scenery_modified();
}

chunk::chunk() noexcept = default;

chunk::~chunk() noexcept
{
    cleanup_lqt();
}

chunk::chunk(chunk&&) noexcept = default;
chunk& chunk::operator=(chunk&&) noexcept = default;

} // namespace floormat
