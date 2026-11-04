#pragma once
#include "grid.hpp"
#include "search-pred.hpp"
#include "pass-mode.hpp"
#include <bit>

namespace floormat {
struct local_coords;
class chunk;
}

namespace floormat::detail::grid {
struct PassGrid;
struct PassPool;

constexpr u32 calc_div_size(u32 div_sizeʹ, u32 bbox_size)
{
    // caller wants cells coarser than the bbox; grid requires div_size <= bbox_size
    auto div_size = div_sizeʹ && div_sizeʹ <= bbox_size ? div_sizeʹ : bbox_size;
    // smaller than a chunk, see Params::validate()
    div_size = div_size < chunk_size_xy / 2 ? div_size : chunk_size_xy / 2;
    return div_size ? std::bit_floor(div_size) : 1; // xxx todo assumes power of two chunk size
}

} // namespace floormat::detail::grid

namespace floormat::Grid::Pass {

using pred = Search::pred;
using BitView = detail::grid::BitView;

struct Params
{
    u32 div_count;
    u32 bbox_size;
    pass_mode min_pass;

    // div_size snaps down to a divisor of chunk_size_xy no larger than bbox_size
    constexpr Params(u32 div_size, pass_mode min_pass): Params{div_size, div_size, min_pass} {}
    constexpr Params(u32 div_size, u32 bbox_size, pass_mode min_pass);

    constexpr u32 div_size() const { return chunk_size_xy >> std::countr_zero(div_count); } // xxx todo assumes power of two chunk size

    [[nodiscard]] Params validate() const;

    bool operator==(const Params&) const noexcept;
};

class Grid
{
    detail::grid::PassGrid* grid;
    detail::grid::PassPool* pool;

public:
    ~Grid() noexcept;
    explicit Grid(detail::grid::PassGrid* grid, detail::grid::PassPool* pool);
    Grid(const Grid&) noexcept = default;
    Grid& operator=(const Grid&) & noexcept = default;

    static u32 get_bitmask_index(u32 x, u32 y, u32 div_count);
    u32 get_bitmask_index_from_coord(intra_coord pos) const;

    Range2D get_coord_from_div(u32 x, u32 y) const;

    bool bit(u32 index) const;
    BitView bits() const;


    u32 div_count() const;
    u64 build_no() const;
    bool is_all_empty() const;

    detail::grid::PassGrid* raw() const noexcept;

    void mark_stale();

    void maybe_mark_stale();

    void build_if_stale();

    static constexpr u32 min_bbox_size = 4;
};

constexpr Params::Params(u32 div_size, u32 bbox_size, pass_mode min_pass):
    bbox_size{bbox_size > Grid::min_bbox_size ? bbox_size : Grid::min_bbox_size},
    min_pass{min_pass}
{
    div_count = chunk_size_xy / detail::grid::calc_div_size(div_size, this->bbox_size);
}

// The pool keeps a copy of the view, so the callable it refers to must outlive the pool.
class Pool final
{
    detail::grid::PassPool* pool;

public:
    explicit Pool(Params params, const pred& predicate);
    ~Pool() noexcept;
    fm_DISABLE_MOVE_COPY(Pool);

    [[nodiscard]] Grid operator[](chunk& c);

    void maybe_mark_stale_all(const world& w);

    void build_if_stale_all();

    const Params& params() const;
    u32 pooled_count() const;
};

} // namespace floormat::Grid::Pass

namespace floormat {
namespace Pass = floormat::Grid::Pass;
} // namespace floormat
