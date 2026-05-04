#pragma once
#include "grid.hpp"
#include "world.hpp"
#include "search-cache.hpp"
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

    chunk* sc = self.w->at(self.coord);
    fm_assert(sc);

    self.neighbors = self.w->neighbors(self.coord);
    self.build_impl(sc, forward<Args>(args)...);
}

template <BitGrid Self, typename... Args>
requires requires(Self& s, chunk* sc, Args&&... a) {
    s.build_impl(sc, forward<Args>(a)...);
}
void GridBase::build_if_stale(this Self& self, Search::cache& cache, Args&&... args)
{
    if (!self.is_stale())
        return;

    fm_assert(cache.contains_chunk(self.coord));
    chunk* sc = cache.try_get_chunk(*self.w, self.coord);
    fm_assert(sc);

    self.neighbors = cache.get_neighbors(*self.w, self.coord);
    self.build_impl(sc, forward<Args>(args)...);
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

} // namespace floormat::detail::grid
