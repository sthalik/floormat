#pragma once
#include "grid.hpp"
#include "world.hpp"
#include "compat/assert.hpp"
#include "compat/hash-table-load-factor.hpp"
#include <gtl/phmap.hpp>

namespace floormat::detail::grid {

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

template <BitGrid Self, typename... Args>
requires requires(Self& s, chunk* sc, Args&&... a) {
    s.build_impl(sc, forward<Args>(a)...);
}
void GridBase::build_if_stale(this Self& self, Args&&... args)
{
    if (!self.is_stale())
        return;

    chunk* sc = self.w->chunk_at_memo(self.coord);
    fm_assert(sc);

    self.neighbors = self.w->neighbors(self.coord);
    self.build_impl(sc, forward<Args>(args)...);
    self.build_no = GridBase::next_build_no();
}

template <typename T>
Pool<T>::Pool(T::Params p) requires BitGrid<T>:
    params{p.validate()}
{
    Hash::set_open_addressing_load_factor(grids);
}

template <typename T>
Pool<T>::~Pool() noexcept
{
    for (auto& [_, g] : grids)
        delete g;
    while (auto* base = freelist.take())
    {
        T* t = static_cast<T*>(base);
        fm_assert(!t->c);
        delete t;
    }
}

template <typename T>
void Pool<T>::put(T* p) requires BitGrid<T>
{
    fm_assert(p);
    fm_assert(!p->c);
    freelist.put(p);
}

template <typename T>
T* Pool<T>::take(chunk& ch) requires BitGrid<T>
{
    if (auto* base = freelist.take())
    {
        T* t = static_cast<T*>(base);
        t->reset_for_reuse(ch, params);
        return t;
    }
    return new T{ch, params};
}

template <typename T>
void check_frame_sync(Pool<T>* pool, T* grid)
{
    fm_assert(pool->frame_no == grid->w->frame_no());
}

template <typename T>
T* pool_subscript(Pool<T>* p, chunk& c)
{
    fm_assert(p->frame_no == c.world().frame_no());
    auto coord = c.coord();
    if (auto it = p->grids.find(coord); it != p->grids.end())
    {
        fm_debug2_assert(it->second);
        return it->second;
    }
    Hash::set_open_addressing_load_factor(p->grids, p->grids.size() + 1);
    auto [it, inserted] = p->grids.try_emplace(coord, nullptr);
    fm_debug_assert(inserted);
    it->second = p->take(c);
    return it->second;
}

template <typename T>
bool grid_bit_at(Pool<T>* pool, T* grid, uint32_t index)
{
    (void)pool;
    //check_frame_sync(pool, grid);
    //fm_assert(index < grid->bitmask.size());
    return grid->bitmask[index];
}

} // namespace floormat::detail::grid
