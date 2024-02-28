#pragma once

#if defined __GLIBCXX__ && defined _GLIBCXX_DEBUG
#include <utility>
#include <iterator>
// regular STL implementation stripped of debug code
namespace floormat::Heap {

template<typename It, typename Compare>
inline void _push_heap(It first,
                       std::iter_difference_t<It> holeIndex,
                       std::iter_difference_t<It> topIndex,
                       std::iter_value_t<It> value,
                       Compare& comp)
{
    std::iter_difference_t<It> parent = (holeIndex - 1) / 2;
    while (holeIndex > topIndex && comp(*(first + parent), value))
    {
        *(first + holeIndex) = move(*(first + parent));
        holeIndex = parent;
        parent = (holeIndex - 1) / 2;
    }
    *(first + holeIndex) = move(value);
}

template<typename It, typename Compare>
void push_heap(It first, It last, Compare comp)
{
    _push_heap(first,
               std::iter_difference_t<It>((last - first) - 1),
               std::iter_difference_t<It>(0),
               move(*(last - 1)),
               comp);
}

template<typename It, typename Compare>
void _adjust_heap(It first,
                  std::iter_difference_t<It> holeIndex,
                  std::iter_difference_t<It> len,
                  std::iter_value_t<It> value,
                  Compare& comp)
{
    const auto topIndex = holeIndex;
    auto secondChild = holeIndex;
    while (secondChild < (len - 1) / 2)
    {
        secondChild = 2 * (secondChild + 1);
        if (comp(*(first + secondChild), *(first + (secondChild - 1))))
            secondChild--;
        *(first + holeIndex) = move(*(first + secondChild));
        holeIndex = secondChild;
    }
    if ((len & 1) == 0 && secondChild == (len - 2) / 2)
    {
        secondChild = 2 * (secondChild + 1);
        *(first + holeIndex) = move(*(first + (secondChild - 1)));
        holeIndex = secondChild - 1;
    }
    _push_heap(first, holeIndex, topIndex, move(value), comp);
}

template<typename It, typename Compare>
void pop_heap(It first, It last, Compare comp)
{
    if (last - first > 1)
    {
        --last;
        auto value = move(*last);
        *last = move(*first);
        _adjust_heap(first,
                     std::iter_difference_t<It>(0),
                     std::iter_difference_t<It>(last - first),
                     move(value),
                     comp);
    }
}

} // namespace floormat::Heap
#else
#include <algorithm>
namespace floormat::Heap {

using std::push_heap;
using std::pop_heap;

} // namespace floormat::Heap
#endif
