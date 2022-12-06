#include "collision.hpp"
#include "compat/assert.hpp"
#include "src/chunk.hpp"
#include "compat/LooseQuadtree-impl.h"
#include <utility>

namespace floormat {

void collision_bb_extractor::ExtractBoundingBox(const collision_bbox* x, BB* bbox)
{
    *bbox = { x->left, x->top, std::int16_t(x->width), std::int16_t(x->height) };
}

collision_iterator::collision_iterator() noexcept : q{nullptr} {}
collision_iterator::collision_iterator(Query* q) noexcept : q{q} {}

collision_iterator::~collision_iterator() noexcept
{
    if (q)
        while (!q->EndOfQuery())
            q->Next();
}

collision_iterator& collision_iterator::operator++() noexcept
{
    fm_debug_assert(q != nullptr && !q->EndOfQuery());
    q->Next();
    return *this;
}

auto collision_iterator::operator++(int) noexcept -> const collision_bbox*
{
    fm_debug_assert(q != nullptr && !q->EndOfQuery());
    auto* bbox = q->GetCurrent();
    fm_debug_assert(bbox != nullptr);
    operator++();
    return bbox;
}

auto collision_iterator::operator*() const noexcept -> const collision_bbox&
{
    return *operator->();
}

auto collision_iterator::operator->() const noexcept -> const collision_bbox*
{
    fm_debug_assert(q != nullptr && !q->EndOfQuery());
    auto* ptr = q->GetCurrent();
    fm_debug_assert(ptr != nullptr);
    return ptr;
}

bool collision_iterator::operator==(const collision_iterator& other) const noexcept
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

collision_iterator::operator bool() const noexcept
{
    return q && !q->EndOfQuery();
}

collision_query::collision_query(collision_query::Query&& q) noexcept : q{std::move(q)} {}
collision_iterator collision_query::begin() noexcept { return collision_iterator{&q}; }
collision_iterator collision_query::end() noexcept { return collision_iterator{nullptr}; }

collision_bbox::operator BB() const noexcept
{
    return BB{left, top, (Short)width, (Short)height};
}

bool collision_bbox::intersects(BB bb) const noexcept
{
    return BB(*this).Intersects(bb);
}

bool collision_bbox::intersects(collision_bbox bb) const noexcept
{
    return BB(*this).Intersects(bb);
}

} // namespace floormat
