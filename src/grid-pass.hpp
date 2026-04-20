#pragma once

/** @file
 *  @brief Per-chunk passability bitmask (one bit per `div_size` cell; critters pass-through). Consumed by cover-mask / pathfinding.
 */

#include "compat/defs.hpp"
#include "tile-defs.hpp"

namespace floormat {
struct local_coords;
class chunk;
}

namespace floormat::detail::grid {
struct Grid;
struct Pool;
}

namespace floormat::Grid::Pass {

struct BitView
{
    uint8_t* data;

    bool read(uint32_t i) const;
    void set(uint32_t i);
    void reset(uint32_t i);
    void write(uint32_t i, bool value);
};

/// `div_size` must divide `chunk_size_xy` (=1024) exactly and be `<= bbox_size`;
/// `validate()` snaps it down to the largest legal divisor.
struct Params
{
    uint32_t div_size = tile_size_xy;
    uint32_t bbox_size = tile_size_xy;

    Params validate() const;

    bool operator==(const Params&) const noexcept;
};

/// Non-owning handle; must not outlive its Pool. Readers assert
/// `pool.frame_no == chunk.world().frame_no()`.
class Grid
{
    detail::grid::Grid* grid;
    detail::grid::Pool* pool;

public:
    ~Grid() noexcept;
    explicit Grid(detail::grid::Grid* grid, detail::grid::Pool* pool);
    Grid(const Grid&) noexcept;
    Grid& operator=(const Grid&) & noexcept;

    static uint32_t get_bitmask_index(uint32_t x, uint32_t y, uint32_t div_count);
    uint32_t get_bitmask_index_from_coord(local_coords local, Vector2b offset) const;

    /// Bbox that @ref build_if_stale actually tested for cell `(x, y)` —
    /// centered at cell center, extent `bbox_size` per axis.
    Range2D get_coord_from_div(uint32_t x, uint32_t y) const;

    /// Freshly-inserted grid is stale and zero-initialized; call
    /// @ref build_if_stale first.
    bool bit(uint32_t index) const;
    BitView bits() const;

    uint32_t div_count() const;

    /// Cascade-marks all 8 neighbors stale.
    void mark_stale();

    /// Checks self + 8 neighbors for pointer swap, `pass_gen_counter` bump, or
    /// `is_passability_modified`. Cascades only on not-stale → stale transition.
    void maybe_mark_stale();

    /// Synchronous; auto-triggers `chunk::ensure_passability` via `rtree()`.
    void build_if_stale();
};

/// Per-frame: `maybe_mark_stale_all(w.frame_no())` then `build_if_stale_all()`
/// before any `pool[c]` access (which asserts frame-no sync).
class Pool final
{
    detail::grid::Pool* pool;

public:
    explicit Pool(Params params);
    ~Pool() noexcept;
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Pool);

    /// Newly-inserted grid is stale until @ref build_if_stale_all or an
    /// explicit `grid.build_if_stale()`.
    [[nodiscard]] Grid operator[](chunk& c);

    /// Stores `frame_no`, sweeps staleness, GCs collected chunks' grids.
    /// Pass `+1` if called before an upcoming `world::increment_frame_no()`.
    void maybe_mark_stale_all(uint64_t frame_no);

    void build_if_stale_all();

    Params params() const;
    uint32_t pooled_count() const;
};

} // namespace floormat::Grid::Pass

namespace floormat {
namespace Pass = floormat::Grid::Pass;
} // namespace floormat
