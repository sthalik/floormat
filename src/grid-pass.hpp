#pragma once
#include "grid.hpp"
#include "search-pred.hpp"

namespace floormat {
struct local_coords;
class chunk;
}

namespace floormat::detail::grid {
struct PassGrid;
}


namespace floormat::Grid::Pass {

using pred = Search::pred;
using BitView = detail::grid::BitView;

const pred& is_passable_without_critters();

struct Params
{
    uint32_t div_size = tile_size_xy;
    uint32_t bbox_size = div_size;

    Params validate() const;

    bool operator==(const Params&) const noexcept;
};

class Grid
{
    detail::grid::PassGrid* grid;
    detail::grid::Pool<detail::grid::PassGrid>* pool;

public:
    ~Grid() noexcept;
    explicit Grid(detail::grid::PassGrid* grid, detail::grid::Pool<detail::grid::PassGrid>* pool);
    Grid(const Grid&) noexcept = default;
    Grid& operator=(const Grid&) & noexcept = default;

    static uint32_t get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count);
    uint32_t get_bitmask_index_from_coord(local_coords local, Vector2b offset) const;

    Range2D get_coord_from_div(uint32_t x, uint32_t y) const;

    bool bit(uint32_t index) const;
    BitView bits() const;


    uint32_t div_count() const;
    uint64_t build_no() const;
    bool is_all_empty() const;

    explicit operator bool() const noexcept;
    detail::grid::PassGrid* raw() const noexcept;

    void mark_stale();

    void maybe_mark_stale();

    void build_if_stale(const pred& predicate);
};

class Pool final
{
    detail::grid::Pool<detail::grid::PassGrid>* pool;

public:
    explicit Pool(Params params);
    ~Pool() noexcept;
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Pool);

    [[nodiscard]] Grid operator[](chunk& c);
    [[nodiscard]] Grid wrap(detail::grid::PassGrid* g) const noexcept;

    void maybe_mark_stale_all(uint64_t frame_no);

    void build_if_stale_all(const pred& predicate);

    Params params() const;
    uint64_t frame_no() const;
    uint32_t pooled_count() const;
};

} // namespace floormat::Grid::Pass

namespace floormat {
namespace Pass = floormat::Grid::Pass;
} // namespace floormat
