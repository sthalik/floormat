#pragma once
#include "grid.hpp"
#include "search-pred.hpp"
#include "pass-mode.hpp"
#include "compat/array-size.hpp"
#include <bit>

namespace floormat {
struct local_coords;
class chunk;
}

namespace floormat::detail::grid {
struct PassGrid;

constexpr auto get_divisor_count()
{
    auto count = 0u;
    for (auto i = 1u; i <= chunk_size_xy; i++)
        if (chunk_size_xy % i == 0)
            count++;
    return count;
}

constexpr auto make_divisor_table()
{
    constexpr auto count = get_divisor_count();
    std::array<uint32_t, count> array;
    uint32_t divisor = 1;
    for (auto i = 0u; i < array_size(array); i++)
    {
        while (chunk_size_xy % divisor)
            divisor++;
        array[i] = divisor++;
    }
    return array;
}

constexpr auto calc_div_size(uint32_t div_sizeʹ, uint32_t bbox_size)
{
    constexpr auto div_sizes = make_divisor_table();
    uint32_t div_size = div_sizeʹ;

    if (!div_sizeʹ || div_sizeʹ > bbox_size) [[unlikely]]
    {
        // caller wants cells coarser than the bbox; grid requires div_size <= bbox_size
        div_size = div_sizes[0];  // smallest divisor as floor
        for (auto i : div_sizes)
            if (i <= bbox_size)
                div_size = i;
            else
                break; // largest ≤ upper
    }
    else if (chunk_size_xy % div_sizeʹ) [[unlikely]]
    {
        // valid magnitude but it doesn't divide the chunk evenly; snap down so cells tile the chunk
        div_size = div_sizes[0];
        for (auto i : div_sizes)
            if (i <= div_sizeʹ)
                div_size = i;
            else
                break;
    }
    return div_size;
}

} // namespace floormat::detail::grid

namespace floormat::Grid::Pass {

using pred = Search::pred;
using BitView = detail::grid::BitView;

struct Params
{
    uint32_t div_count;
    uint32_t bbox_size;
    pass_mode min_pass;

    // div_size snaps down to a divisor of chunk_size_xy no larger than bbox_size
    constexpr Params(uint32_t div_size, pass_mode min_pass): Params{div_size, div_size, min_pass} {}
    constexpr Params(uint32_t div_size, uint32_t bbox_size, pass_mode min_pass);

    constexpr uint32_t div_size() const
    {
        if constexpr (std::has_single_bit(chunk_size_xy))
            return chunk_size_xy >> std::countr_zero(div_count);
        else
            return chunk_size_xy / div_count;
    }

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
    uint32_t get_bitmask_index_from_coord(intra_coord pos) const;

    Range2D get_coord_from_div(uint32_t x, uint32_t y) const;

    bool bit(uint32_t index) const;
    BitView bits() const;


    uint32_t div_count() const;
    uint64_t build_no() const;
    bool is_all_empty() const;

    detail::grid::PassGrid* raw() const noexcept;

    void mark_stale();

    void maybe_mark_stale();

    void build_if_stale(const pred& predicate);

    static constexpr uint32_t min_bbox_size = 4;
};

constexpr Params::Params(uint32_t div_size, uint32_t bbox_size, pass_mode min_pass):
    bbox_size{bbox_size > Grid::min_bbox_size ? bbox_size : Grid::min_bbox_size},
    min_pass{min_pass}
{
    div_count = chunk_size_xy / detail::grid::calc_div_size(div_size, this->bbox_size);
}

// The build predicate is not part of a grid's staleness key. Use each Pool
// with a single predicate, or consumers read bitmaps built for another one.
class Pool final
{
    detail::grid::Pool<detail::grid::PassGrid>* pool;
    uint64_t checked_pass_gen = (uint64_t)-1;

public:
    explicit Pool(Params params);
    ~Pool() noexcept;
    fm_DISABLE_MOVE_COPY(Pool);

    [[nodiscard]] Grid operator[](chunk& c);
    [[nodiscard]] Grid wrap(detail::grid::PassGrid* g) const noexcept;

    void maybe_mark_stale_all(uint64_t frame_no);

    void build_if_stale_all(const pred& predicate);

    const Params& params() const;
    uint64_t frame_no() const;
    uint32_t pooled_count() const;
};

} // namespace floormat::Grid::Pass

namespace floormat {
namespace Pass = floormat::Grid::Pass;
} // namespace floormat
