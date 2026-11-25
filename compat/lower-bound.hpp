#pragma once
#include "range-access.hpp"

namespace floormat {

template <random_access_iterator It, sized_sentinel_for<It> S, class T, class Compare>
constexpr It lower_bound(It first, S last, const T& value, Compare comp)
{
    using difference_type = decltype(last - first);
    auto count = (usize)(last - first);

    while (count > 0)
    {
        usize step = count / 2;
        It it = first + (difference_type)step;

        if (comp(*it, value))
        {
            first = ++it;
            count -= step + 1;
        }
        else
            count = step;
    }
    return first;
}

// Rejects rvalue ranges, whose iterators would outlive them.
template <random_access_range R, class T, class Compare>
requires std::is_lvalue_reference_v<R> && sized_sentinel_for<sentinel_t<R>, iterator_t<R>>
constexpr iterator_t<R> lower_bound(R&& r, const T& value, Compare comp)
{
    return floormat::lower_bound(floormat::begin(r), floormat::end(r), value, comp);
}

} // namespace floormat
