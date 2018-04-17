#pragma once

#include "common.hpp"

#include <tuple>

namespace hilbert_detail {

using namespace world_detail;

class hilbert
{
    //static void rot(long s, Len2& xy, Len rx, Len ry);
    //static Ord from_2d_(Len2 xy);
    static Ord const* make_LUT_2d_to_1d();

    static Ord const* LUT_2d_to_1d;
public:
    hilbert() = delete;
    static Ord lookup_2d_to_1d(Len2 xy);
};

} // ns hilbert_detail

using hilbert = hilbert_detail::hilbert;
