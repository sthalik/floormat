#pragma once
#include "grid.hpp"

namespace floormat {
struct local_coords;
class chunk;
}

namespace floormat::detail::grid {
struct CoverGrid;
struct CoverCell;
} // namespace


namespace floormat::Grid::Cover {

inline constexpr uint32_t octant_count = 32;

struct Params
{
    uint32_t div_size = tile_size_xy;

    Params validate() const;
    bool operator==(const Params&) const noexcept;
};

class Grid
{
    detail::grid::CoverGrid* grid;
    detail::grid::Pool<detail::grid::CoverGrid>* pool;

public:
    ~Grid() noexcept;
    explicit Grid(detail::grid::CoverGrid* grid, detail::grid::Pool<detail::grid::CoverGrid>* pool);
    Grid(const Grid&) noexcept;
    Grid& operator=(const Grid&) & noexcept;

    static uint32_t get_cell_index(uint32_t x, uint32_t y, uint32_t div_count);
    uint32_t get_cell_index_from_coord(local_coords local, Vector2b offset) const;

    const detail::grid::CoverCell& cell(uint32_t index) const;
    uint8_t distance(uint32_t index, uint32_t octant) const;

    uint32_t div_count() const;
    uint64_t build_no() const;

    explicit operator bool() const noexcept;
    detail::grid::CoverGrid* raw() const noexcept;

    void mark_stale();
    void maybe_mark_stale();
    void build_if_stale();

    bool ensure_octant(uint32_t k);
    bool fill_next_unfilled();
    uint32_t built_octants() const;
};

class Pool final
{
    detail::grid::Pool<detail::grid::CoverGrid>* pool;

public:
    explicit Pool(Params params);
    ~Pool() noexcept;
    fm_DISABLE_MOVE_COPY(Pool);

    [[nodiscard]] Grid operator[](chunk& c);
    [[nodiscard]] Grid wrap(detail::grid::CoverGrid* g) const noexcept;

    void maybe_mark_stale_all(uint64_t frame_no);
    void build_if_stale_all();

    Params params() const;
    uint64_t frame_no() const;
    uint32_t pooled_count() const;
};

} // namespace floormat::Grid::Cover

namespace floormat {
namespace Cover = floormat::Grid::Cover;
} // namespace floormat
