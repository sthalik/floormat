#pragma once

#if defined __GLIBCXX__ && defined _GLIBCXX_DEBUG
// Heap implementation -*- C++ -*-

// Copyright (C) 2001-2024 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/*
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * Copyright (c) 1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */
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
