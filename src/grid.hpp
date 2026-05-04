#pragma once
#include "compat/defs.hpp"
#include "compat/function2.fwd.hpp"
#include "global-coords.hpp"
#include <array>
#include <concepts>
#include <gtl/phmap.hpp>

namespace floormat {
class chunk;
class world;
struct local_coords;
} // namespace floormat

namespace floormat::Search { struct cache; }

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

    explicit GridBase(chunk& ch);
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(GridBase);

    bool is_stale() const;
    void mark_stale();
    void reset_base_for_reuse(chunk& ch);
    void maybe_mark_stale();
    void maybe_mark_stale(Search::cache& cache);

    static uint32_t pack_bit_index(uint32_t i, uint32_t j, uint32_t div_count);
    static std::pair<uint32_t, uint8_t> byte_and_mask(uint32_t bit_index);

protected:
    ~GridBase() noexcept = default;

private:
    void maybe_mark_stale_impl(fu2::function_view<chunk*(chunk_coords_) const> const& at_chunk);

public:
    template <BitGrid Self, typename... Args>
    requires requires(Self& s, chunk* sc, Args&&... a) {
        s.build_impl(sc, forward<Args>(a)...);
    }
    void build_if_stale(this Self& self, Args&&... args);

    template <BitGrid Self, typename... Args>
    requires requires(Self& s, chunk* sc, Args&&... a) {
        s.build_impl(sc, forward<Args>(a)...);
    }
    void build_if_stale(this Self& self, Search::cache& cache, Args&&... args);
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

template <typename T>
struct Pool
{
    free_list freelist;
    gtl::flat_hash_map<chunk_coords_, T*, Hash::chunk_coord_hasher> grids{};
    T::Params params;
    uint64_t frame_no = (uint64_t)-1;

    explicit Pool(T::Params p) requires BitGrid<T>;
    ~Pool() noexcept;
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Pool);

    void put(T* p) requires BitGrid<T>;
    T* take(chunk& ch) requires BitGrid<T>;
};

struct BitView
{
    uint8_t* data;

    bool read(uint32_t i) const;
    void set(uint32_t i);
    void reset(uint32_t i);
    void write(uint32_t i, bool value);
};

} // namespace floormat::detail::grid
