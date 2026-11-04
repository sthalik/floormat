#pragma once
#include "grid.hpp"

namespace floormat {
struct local_coords;
class chunk;
struct Ns;
}

namespace floormat::detail::grid {
struct CoverGrid;
struct CoverPool;
} // namespace

namespace floormat::Grid::Pass { class Pool; }

namespace floormat::Grid::Cover {

inline constexpr u32 octant_count = 32;
inline constexpr u16 blocked = (u16)-1;
constexpr inline u32 max_div_size = 64;

// Within 0.7° of k*11.25°. Octant k's distances run along it.
Vector2 octant_direction(u32 k);

struct Params
{
    u32 div_size;

    constexpr explicit Params(u32 div_size): div_size{div_size} {}

    [[nodiscard]] Params validate() const;
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

    static u32 get_cell_index(u32 x, u32 y, u32 div_count);
    u32 get_cell_index_from_coord(intra_coord pos) const;

    // From the cell's center in div_size units, rounded. div_count() means no hit within
    // chunk_size_xy - div_size/2 px. A cell whose shoot_through pass bit is clear holds `blocked`.
    u16 distance(u32 index, u32 octant) const;

    u32 div_count() const;
    u64 build_no() const;

    explicit operator bool() const noexcept;
    detail::grid::CoverGrid* raw() const noexcept;

    bool is_stale() const;
    void mark_stale();
    void maybe_mark_stale();
    void build_if_stale();

    bool ensure_octant(u32 k);
    bool fill_next_unfilled();
    u32 built_octants() const;
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

    void maybe_mark_stale_all(const world& w);
    void build_if_stale_all();

    Params params() const;
    u32 pooled_count() const;
    // since the pool was made
    u32 octant_fills() const;
    Ns octant_fill_time() const;
};

} // namespace floormat::Grid::Cover

namespace floormat {
namespace Cover = floormat::Grid::Cover;
} // namespace floormat
