#pragma once
#include "grid.hpp"
#include "world.hpp"
#include "compat/assert.hpp"
#include "compat/atomic.hpp"
#include <cr/GrowableArray.h>

namespace floormat::Grid::detail {

template <typename T>
struct Pool : PoolBase
{
    T::Params params;

    explicit Pool(T::Params p) requires BitGrid<T>;
    ~Pool() noexcept;
    fm_DISABLE_MOVE_COPY(Pool);

    T* take(chunk& ch) requires BitGrid<T>;
};

constexpr inline std::array<Vector2b, 16> ring2_offsets = {{
    {-2, -2}, {-1, -2}, { 0, -2}, { 1, -2}, { 2, -2},
    {-2, -1},                               { 2, -1},
    {-2,  0},                               { 2,  0},
    {-2,  1},                               { 2,  1},
    {-2,  2}, {-1,  2}, { 0,  2}, { 1,  2}, { 2,  2},
}};

template <BitGrid Self, typename... Args>
requires requires(Self& s, chunk* sc, Args&&... a) {
    s.build_impl(sc, forward<Args>(a)...);
}
void GridBase::build_if_stale(this Self& self, Args&&... args)
{
    if (!self.needs_rebuild())
        return;

    fm_debug3_assert(self.c);
    fm_assert(!self.c->world().is_thread_pool_running());
    self.build_without_build_no(forward<Args>(args)...);
    self.build_no = GridBase::next_build_no();
}

template <BitGrid Self, typename... Args>
requires requires(Self& s, chunk* sc, Args&&... a) {
    s.build_impl(sc, forward<Args>(a)...);
}
void GridBase::build_without_build_no(this Self& self, Args&&... args)
{
    chunk* sc = self.c;
    fm_debug3_assert(sc);
    const auto& w = sc->world();
    fm_assert(!w.is_thread_pool_running() || w.running_job_kind() == job_kind::build_grids);

    self.neighbors = sc->world().neighbors(self.coord);
    self.build_impl(sc, forward<Args>(args)...);
    self.built_pass_gen = w.pass_gen();
}

template <typename T>
Pool<T>::Pool(T::Params p) requires BitGrid<T>:
    params{p.validate()}
{
}

template <typename T>
Pool<T>::~Pool() noexcept
{
    detach_all();
    for (auto* g : grids_in_use)
        delete static_cast<T*>(g);
    grids_in_use = {};
    while (auto* base = freelist.take())
    {
        T* t = static_cast<T*>(base);
        fm_assert(!t->c);
        delete t;
    }
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

// Each call looks up as many as 8 chunks, so only accessors that a query calls once per chunk run this check.
template <typename T>
void check_current([[maybe_unused]] T* grid)
{
    fm_debug3_assert(grid->is_marked_stale() || grid->is_current() && !grid->needs_rebuild());
}

template <typename T>
T* pool_subscript(Pool<T>* p, chunk& c)
{
    if (auto* g = p->find(c))
        return static_cast<T*>(g);
    T* t = p->take(c);
    p->attach(c, t);
    return t;
}

template <typename T>
const T* find_built_grid(const Pool<T>* p, const chunk& c)
{
    const auto* g = static_cast<const T*>(p->find(c));
    if (!g)
        return nullptr;
    // Stores only when clear, so threads reading the grid in one frame don't bounce its cache line.
    if (!atomic_load(&g->is_requested_by_reader, memory_order::relaxed))
        atomic_store(&g->is_requested_by_reader, 1, memory_order::relaxed);
    if (g->needs_rebuild())
        return nullptr;
    else
    {
        check_current(g);
        return g;
    }
}

template <typename T>
void list_grid_if_stale(T* grid, Array<grid_to_build>& list)
{
    if (!grid->is_listed_to_be_built && grid->needs_rebuild())
    {
        grid->is_listed_to_be_built = true;
        arrayAppend(list, grid_to_build{grid, T::build_grid});
    }
}

template <typename T>
void list_requested_stale_grids(Pool<T>* p, Array<grid_to_build>& list)
{
    for (auto* g : p->grids_in_use)
        if (atomic_load(&g->is_requested_by_reader, memory_order::relaxed))
        {
            list_grid_if_stale(static_cast<T*>(g), list);
            atomic_store(&g->is_requested_by_reader, 0, memory_order::relaxed);
        }
}

} // namespace floormat::Grid::detail
