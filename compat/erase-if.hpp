#pragma once
#include "range-access.hpp"
#include "container-traits.hpp"

namespace floormat {

// Removes the elements for which pred is true, keeps the order of the rest, and returns how many it removed.
// C needs a container_traits specialization.
template<typename C, typename Pred> requires range<C> && sized_sentinel_for<sentinel_t<C>, iterator_t<C>>
constexpr usize erase_if(C& c, Pred&& pred)
{
    auto first = floormat::begin(c);
    const auto last = floormat::end(c);
    for (; first != last; ++first)
        if (pred(*first))
            break;
    auto out = first;
    if (first != last)
    {
        while (++first != last)
        {
            if (!pred(*first))
            {
                *out = floormat::move(*first);
                ++out;
            }
        }
    }
    const auto removed = usize(last - out);
    if (removed != 0)
        container_traits<C>::remove_suffix(c, removed);
    return removed;
}

} // namespace floormat
