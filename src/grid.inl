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

inline GridBase* PoolBase::find(const chunk& c) const noexcept
{
    if (!c._grid_slots)
        return nullptr;
    const auto& s = c._grid_slots->slots;
    return slot_index < s.size() ? s[slot_index] : nullptr;
}

template <BitGrid Self, typename... Args>
requires requires(Self& s, chunk* sc, Args&&... a) {
    s.build_impl(sc, forward<Args>(a)...);
}
void GridBase::build_if_stale(this Self& self, Args&&... args)
{
    if (!self.is_stale())
        return;

    chunk* sc = self.c;
    fm_debug3_assert(sc);

    self.neighbors = sc->world().neighbors(self.coord);
    self.build_impl(sc, forward<Args>(args)...);
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
    for (auto* g : live)
        delete static_cast<T*>(g);
    live = {};
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

template <typename T>
void check_frame_sync([[maybe_unused]] Pool<T>* pool, [[maybe_unused]] T* grid)
{
    fm_debug3_assert(pool->frame_no == grid->c->world().frame_no());
}

template <typename T>
T* pool_subscript(Pool<T>* p, chunk& c)
{
    fm_debug3_assert(p->frame_no == c.world().frame_no());
    if (auto* g = p->find(c))
        return static_cast<T*>(g);
    T* t = p->take(c);
    p->attach(c, t);
    return t;
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
