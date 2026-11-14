#pragma once
#include "grid.hpp"
#include "world.hpp"
#include "compat/assert.hpp"

namespace floormat::detail::grid {

template <typename T>
struct Pool : PoolBase
{
    T::Params params;

    explicit Pool(T::Params p) requires BitGrid<T>;
    ~Pool() noexcept;
    fm_DISABLE_MOVE_COPY(Pool);

    T* take(chunk& ch) requires BitGrid<T>;
};

inline GridBase* PoolBase::find(chunk& c) const noexcept
{
    if (!c._grid_slots)
        return nullptr;
    const auto& s = c._grid_slots->slots;
    return slot_index < s.size() ? s[slot_index] : nullptr;
}

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

    chunk* sc = self.c;
    fm_debug3_assert(sc);

    self.neighbors = sc->world().neighbors(self.coord);
    self.build_impl(sc, forward<Args>(args)...);
    self.built_pass_gen = sc->world().pass_gen();
    self.build_no = GridBase::next_build_no();
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

// Up to 8 chunk lookups per call, so only accessors a query calls once per chunk check, not per cell.
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

} // namespace floormat::detail::grid
