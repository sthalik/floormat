#pragma once
#include "compat/defs.hpp"
#include "compat/function-view-fwd.hpp"
#include "global-coords.hpp"
#include "tile-defs.hpp"
#include "intra-coord-fwd.hpp"
#include <array>
#include <concepts>
#include <cr/Array.h>

namespace floormat {
class chunk;
class world;
struct local_coords;
} // namespace floormat

namespace floormat::Grid::detail {

struct GridBase;
struct PoolBase;

// Returns div_count² rounded up to a multiple of 64, so clear_rect_*() never accesses past a small grid's end.
constexpr u32 bitmask_size(u32 div_count)
{
    return (div_count * div_count + 63) & ~63u;
}

template <typename T>
concept BitGrid = requires(T t, const T& ct, chunk& ch, T::Params p) {
    requires std::derived_from<T, GridBase>;
    typename T::Params;
    { t.reset_for_reuse(ch, p) } -> std::same_as<void>;
    { ct.needs_rebuild() } -> std::same_as<bool>;
};

struct GridBase
{
    GridBase* next = nullptr;
    PoolBase* pool = nullptr;
    chunk* c;
    chunk_coords_ coord;
    std::array<chunk*, 8> neighbors{};
    u64 built_pass_gen = 0; // world::pass_gen() after the last build, or 0 while marked stale
    u64 build_no = 0;
    u32 in_use_index = (u32)-1;

    explicit GridBase(chunk& ch);
    fm_DISABLE_MOVE_COPY(GridBase);

    /// @brief True after construction, reuse or mark_stale(), until build_if_stale() rebuilds the grid.
    bool is_marked_stale() const;
    void mark_stale();
    void reset_base_for_reuse(chunk& ch);
    bool is_current() const;

    static u32 pack_bit_index(u32 i, u32 j, u32 div_count);
    static u32 pack_bit_index_from_coord(intra_coord pos, u32 div_count);
    static std::pair<u32, usize> word_and_mask(u32 bit_index);
    static u64 next_build_no();

    template <BitGrid Self, typename... Args>
    requires requires(Self& s, chunk* sc, Args&&... a) {
        s.build_impl(sc, forward<Args>(a)...);
    }
    void build_if_stale(this Self& self, Args&&... args);

protected:
    ~GridBase() noexcept = default;
};

class free_list
{
    GridBase* list = nullptr;
public:
    GridBase* take();
    void put(GridBase* p);
    bool is_empty() const;
    u32 size() const;
};

struct grid_slots
{
    Array<GridBase*> slots;

    grid_slots() noexcept;
    ~grid_slots() noexcept;
    fm_DISABLE_MOVE_COPY(grid_slots);
};

struct PoolBase
{
    free_list freelist;
    Array<GridBase*> grids_in_use;
    const u32 slot_index;
    bool reads_ring2 = false;

    PoolBase();
    ~PoolBase() noexcept;
    fm_DISABLE_MOVE_COPY(PoolBase);

    GridBase* find(chunk& c) const noexcept;
    void attach(chunk& c, GridBase* g);
    void release(GridBase* g) noexcept;
    void detach_all() noexcept;
};

/// @brief Call after writing a new pass_gen() stamp into the chunk at @p coord.
void mark_grids_stale(world& w, chunk_coords_ coord) noexcept;
void cascade_mark_neighbors_stale(chunk_coords_ coord, function_view<GridBase*(chunk_coords_)> find);

template <typename T> struct Pool;

struct BitView
{
    usize* data;

    bool read(u32 i) const;
    void set(u32 i);
    void reset(u32 i);
    void write(u32 i, bool value);
};

} // namespace floormat::Grid::detail
