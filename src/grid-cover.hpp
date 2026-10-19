#pragma once
#include "grid.hpp"

namespace floormat {
struct local_coords;
class chunk;
}

namespace floormat::detail::grid {
struct CoverGrid;
struct CoverPool;
} // namespace

namespace floormat::Grid::Pass { class Pool; }

namespace floormat::Grid::Cover {

inline constexpr uint32_t octant_count = 32;
inline constexpr uint16_t blocked = (uint16_t)-1;

struct Params
{
    uint32_t div_size;

    constexpr explicit Params(uint32_t div_size): div_size{div_size} {}

    Params validate() const;
    bool operator==(const Params&) const noexcept;
};

class Grid
{
    detail::grid::CoverGrid* grid;

public:
    ~Grid() noexcept;
    explicit Grid(detail::grid::CoverGrid* grid);
    Grid(const Grid&) noexcept;
    Grid& operator=(const Grid&) & noexcept;

    static uint32_t get_cell_index(uint32_t x, uint32_t y, uint32_t div_count);
    uint32_t get_cell_index_from_coord(intra_coord pos) const;

    // From the cell's center in div_size units, rounded. div_count() means no hit within
    // chunk_size_xy - div_size/2 px. A cell whose shoot_through pass bit is clear holds `blocked`.
    uint16_t distance(uint32_t index, uint32_t octant) const;

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
    detail::grid::CoverPool* pool;

public:
    // pass_pool must outlive this pool and use Search::without_critters(), the cover rays' predicate
    explicit Pool(Params params, Pass::Pool& pass_pool);
    ~Pool() noexcept;
    fm_DISABLE_MOVE_COPY(Pool);

    [[nodiscard]] Grid operator[](chunk& c);
    [[nodiscard]] Grid wrap(detail::grid::CoverGrid* g) const noexcept;

    void maybe_mark_stale_all(const world& w);
    void build_if_stale_all();

    Params params() const;
    uint32_t pooled_count() const;
};

} // namespace floormat::Grid::Cover

namespace floormat {
namespace Cover = floormat::Grid::Cover;
} // namespace floormat
