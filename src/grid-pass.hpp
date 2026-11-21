#pragma once
#include "grid.hpp"
#include "pass-mode.hpp"
#include <bit>

namespace floormat {
struct local_coords;
class chunk;
}

namespace floormat::Grid::Pass { struct Params; }

namespace floormat::Grid::detail {
struct PassGrid;
struct PassPool;

struct cell_range
{
    i32 i_lo, i_hi, j_lo, j_hi;
    bool is_empty() const { return i_lo > i_hi || j_lo > j_hi; }
};

// The cells whose bit a rect in the chunk's frame clears. With min_pass == pass the rect blocks the box at
// their node, otherwise at one of their positions.
class inflated_cells
{
    float half_div_minus_half_tile, grow_lo, grow_hi, inv_div;
    i32 div_count;

public:
    explicit inflated_cells(const Pass::Params& params);
    cell_range operator()(float x0, float y0, float x1, float y1) const;
};

constexpr u32 calc_div_size(u32 div_sizeʹ, u32 bbox_size)
{
    // caller wants cells coarser than the bbox; grid requires div_size <= bbox_size
    auto div_size = div_sizeʹ && div_sizeʹ <= bbox_size ? div_sizeʹ : bbox_size;
    // smaller than a chunk, see Params::validate()
    div_size = div_size < chunk_size_xy / 2 ? div_size : chunk_size_xy / 2;
    return div_size ? std::bit_floor(div_size) : 1; // xxx todo assumes power of two chunk size
}

} // namespace floormat::Grid::detail

namespace floormat::Grid::Pass {

using BitView = detail::BitView;

struct Params
{
    u32 div_count;
    u32 bbox_size;
    pass_mode min_pass;

    // div_size snaps down to a divisor of chunk_size_xy no larger than bbox_size
    constexpr Params(u32 div_size, pass_mode min_pass): Params{div_size, div_size, min_pass} {}
    constexpr Params(u32 div_size, u32 bbox_size, pass_mode min_pass);

    // xxx todo assumes power of two chunk size
    constexpr u32 div_size() const { return chunk_size_xy >> std::countr_zero(div_count); }
    constexpr u32 node_offset() const { return bbox_size / 2 % div_size(); }

    [[nodiscard]] Params validate() const;

    bool operator==(const Params&) const noexcept;
};

class Grid
{
    detail::PassGrid* grid;
    detail::PassPool* pool;

public:
    ~Grid() noexcept;
    explicit Grid(detail::PassGrid* grid, detail::PassPool* pool);
    Grid(const Grid&) noexcept = default;
    Grid& operator=(const Grid&) & noexcept = default;

    static u32 get_bitmask_index(u32 x, u32 y, u32 div_count);
    u32 get_bitmask_index_from_coord(intra_coord pos) const;

    bool bit(u32 index) const;
    BitView bits() const;


    u32 div_count() const;
    u64 build_no() const;
    /// @brief world::pass_gen() when build_if_stale() last rebuilt the grid, or 0 while it is marked stale.
    u64 built_pass_gen() const;
    /// @brief True when the grid was marked stale and build_if_stale() hasn't rebuilt it since. A grid that
    /// Pool::operator[] creates starts out marked.
    bool is_marked_stale() const;
    /// @brief True when the grid is marked stale, or its chunk or a neighbor changed after its build.
    bool is_stale() const;
    /// @param pass_gen What built_pass_gen() returned when the caller read the grid's data.
    /// @return True when built_pass_gen() no longer equals @p pass_gen, or is_stale() holds.
    bool is_stale(u64 pass_gen) const;
    bool is_all_empty() const;

    detail::PassGrid* raw() const noexcept;

    void mark_stale();

    void build_if_stale();

    static constexpr u32 min_bbox_size = 4;
};

constexpr Params::Params(u32 div_size, u32 bbox_size, pass_mode min_pass):
    bbox_size{bbox_size > Grid::min_bbox_size ? bbox_size : Grid::min_bbox_size},
    min_pass{min_pass}
{
    div_count = chunk_size_xy / detail::calc_div_size(div_size, this->bbox_size);
}

// Its grids leave out critters and dynamic scenery, which Grid::Object::Pool holds.
class Pool final
{
    detail::PassPool* pool;

public:
    explicit Pool(Params params);
    ~Pool() noexcept;
    fm_DISABLE_MOVE_COPY(Pool);

    [[nodiscard]] Grid operator[](chunk& c);

    void build_if_stale_all();

    const Params& params() const;
    u32 free_grid_count() const;
};

} // namespace floormat::Grid::Pass

namespace floormat {
namespace Pass = floormat::Grid::Pass;
} // namespace floormat
