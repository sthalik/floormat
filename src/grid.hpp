#pragma once
#include "compat/defs.hpp"
#include "compat/function2.fwd.hpp"
#include "global-coords.hpp"
#include "tile-defs.hpp"
#include <array>
#include <concepts>

namespace floormat {
class chunk;
class world;
struct local_coords;
inline constexpr uint32_t chunk_size_xy = (uint32_t)tile_size_xy * (uint32_t)TILE_MAX_DIM;
} // namespace floormat

namespace floormat::detail::grid {

struct GridBase;

template <typename T>
concept BitGrid = requires(T t, chunk& ch, T::Params p) {
    requires std::derived_from<T, GridBase>;
    typename T::Params;
    { t.reset_for_reuse(ch, p) } -> std::same_as<void>;
};

struct GridBase
{
    GridBase* next = nullptr;
    chunk* c;
    world* w;
    chunk_coords_ coord;
    std::array<chunk*, 8> neighbors{};
    std::array<uint32_t, 9> versions;   // [0..7]=neighbors, [8]=self; -1 = stale
    uint64_t build_no = 0;

    explicit GridBase(chunk& ch);
    fm_DISABLE_MOVE_COPY(GridBase);

    bool is_stale() const;
    void mark_stale();
    void reset_base_for_reuse(chunk& ch);
    void maybe_mark_stale();

    static uint32_t pack_bit_index(uint32_t i, uint32_t j, uint32_t div_count);
    static uint32_t pack_bit_index_from_coord(local_coords local, Vector2b offset, uint32_t div_size, uint32_t div_count);
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

private:
    void maybe_mark_stale_impl(fu2::function_view<chunk*(chunk_coords_) const> const& at_chunk);
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
