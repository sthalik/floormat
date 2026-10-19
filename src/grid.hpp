#pragma once
#include "compat/defs.hpp"
#include "compat/function2.fwd.hpp"
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

namespace floormat::detail::grid {

struct GridBase;
struct PoolBase;

template <typename T>
concept BitGrid = requires(T t, chunk& ch, T::Params p) {
    requires std::derived_from<T, GridBase>;
    typename T::Params;
    { t.reset_for_reuse(ch, p) } -> std::same_as<void>;
};

struct GridBase
{
    GridBase* next = nullptr;
    PoolBase* pool = nullptr;
    chunk* c;
    chunk_coords_ coord;
    std::array<chunk*, 8> neighbors{};
    std::array<uint64_t, 9> versions;   // [0..7]=neighbors, [8]=self; -1 = stale
    uint64_t build_no = 0;
    uint32_t live_index = (uint32_t)-1;

    explicit GridBase(chunk& ch);
    fm_DISABLE_MOVE_COPY(GridBase);

    bool is_stale() const;
    void mark_stale();
    void reset_base_for_reuse(chunk& ch);
    bool versions_match() const;
    void maybe_mark_stale();

    static uint32_t pack_bit_index(uint32_t i, uint32_t j, uint32_t div_count);
    static uint32_t pack_bit_index_from_coord(intra_coord pos, uint32_t div_count);
    static Range2D coord_range_from_div(uint32_t x, uint32_t y, uint32_t div_size, uint32_t bbox_size);
    static std::pair<uint32_t, uint8_t> byte_and_mask(uint32_t bit_index);
    static uint64_t next_build_no();

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
    uint32_t size() const;
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
    Array<GridBase*> live;
    const uint32_t slot_index;
    uint64_t checked_pass_gen = (uint64_t)-1;

    PoolBase();
    ~PoolBase() noexcept;
    fm_DISABLE_MOVE_COPY(PoolBase);

    GridBase* find(chunk& c) const noexcept;
    void attach(chunk& c, GridBase* g);
    void release(GridBase* g) noexcept;
    void detach_all() noexcept;
    bool update_checked_pass_gen(const world& w) noexcept;
};

void cascade_mark_neighbors_stale(chunk_coords_ coord, fu2::function_view<GridBase*(chunk_coords_)> find);

template <typename T> struct Pool;

struct BitView
{
    uint8_t* data;

    bool read(uint32_t i) const;
    void set(uint32_t i);
    void reset(uint32_t i);
    void write(uint32_t i, bool value);
};

} // namespace floormat::detail::grid
