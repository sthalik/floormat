#pragma once
#include "compat/LooseQuadtree-impl.h"
#include "src/pass-mode.hpp"

namespace floormat {

template<typename Num, typename BB, typename BBE>
struct collision_iterator final
{
    using Query = typename loose_quadtree::LooseQuadtree<Num, BB, BBE>::Query;

    explicit collision_iterator() noexcept : q{nullptr} {}
    explicit collision_iterator(Query* q) noexcept : q{q} {}
    ~collision_iterator() noexcept
    {
        if (q)
            while (!q->EndOfQuery())
                q->Next();
    }
    collision_iterator& operator++() noexcept
    {
        fm_debug_assert(q != nullptr && !q->EndOfQuery());
        q->Next();
        return *this;
    }
    const BB* operator++(int) noexcept
    {
        fm_debug_assert(q != nullptr && !q->EndOfQuery());
        auto* bbox = q->GetCurrent();
        fm_debug_assert(bbox != nullptr);
        operator++();
        return bbox;
    }
    const BB& operator*() const noexcept { return *operator->(); }
    const BB* operator->() const noexcept
    {
        fm_debug_assert(q != nullptr && !q->EndOfQuery());
        auto* ptr = q->GetCurrent();
        fm_debug_assert(ptr != nullptr);
        return ptr;
    }
    bool operator==(const collision_iterator& other) const noexcept
    {
        if (q && !other.q) [[likely]]
            return q->EndOfQuery();
        else if (!q && !other.q)
            return true;
        else if (!q)
            return other.q->EndOfQuery();
        else
            return q == other.q;
    }
    operator bool() const noexcept { return q && !q->EndOfQuery(); }

private:
    Query* q;
};

template<typename Num, typename BB, typename BBE>
struct collision_query
{
    using Query = typename loose_quadtree::LooseQuadtree<Num, BB, BBE>::Query;
    collision_query(Query&& q) noexcept : q{std::move(q)} {}
    ~collision_query() noexcept { while (!q.EndOfQuery()) q.Next(); }
    collision_iterator<Num, BB, BBE> begin() noexcept { return collision_iterator<Num, BB, BBE>{&q}; }
    static collision_iterator<Num, BB, BBE> end() noexcept { return collision_iterator<Num, BB, BBE>{nullptr}; }
    operator bool() const noexcept { return !q.EndOfQuery(); }

private:
    Query q;
};

} // namespace floormat
