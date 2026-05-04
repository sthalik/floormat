#pragma once
#include "compat/defs.hpp"
#include "grid-pass.hpp"
#include <array>
#include <cr/Array.h>
#include <cr/BitArray.h>
#include <mg/Vector2.h>

namespace floormat {
class world;
class chunk;
struct point;
struct local_coords;
struct chunk_coords_;
} // namespace floormat

namespace floormat::Search {

struct cache
{
    struct chunk_cache { class chunk* chunk = nullptr; };

    Vector2ui size;
    Vector2i start{(int)((1u << 31) - 1)};
    uint32_t div_size_;
    uint32_t div_count_;
    Array<chunk_cache> array;
    /// `nullptr` = unresolved, `(PassGrid*)-1` = looked-up-missing, else valid.
    Array<detail::grid::PassGrid*> grids;
    Array<uint32_t> indexes;
    BitArray exists;

    explicit cache(uint32_t div_size);
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(cache);

    size_t get_chunk_index(Vector2i chunk) const;
    static size_t get_chunk_index(Vector2i start, Vector2ui size, Vector2i coord);
    size_t get_tile_index(local_coords local, Vector2b offset) const;
    static Vector2ui get_size_to_allocate(uint32_t max_dist);
    bool contains_chunk(chunk_coords_ ch) const;

    void allocate(point from, uint32_t max_dist);
    void add_index(size_t chunk_index, size_t tile_index, uint32_t index);
    void add_index(point pt, uint32_t index);
    uint32_t lookup_index(size_t chunk_index, size_t tile_index);
    chunk* try_get_chunk(world& w, chunk_coords_ ch);
    Grid::Pass::Grid try_get_grid(world& w, Grid::Pass::Pool& pool, chunk_coords_ ch);

    std::array<chunk*, 8> get_neighbors(world& w, chunk_coords_ ch0);

    bool is_passable_for_bbox(world& w, Grid::Pass::Pool& pool, point pt, const pred& p);
    bool is_passable_between_diag(world& w, Grid::Pass::Pool& pool, point a, point b, const pred& p);
};

} // namespace floormat::Search
