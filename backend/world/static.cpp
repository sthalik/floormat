#include "static.hpp"

#include <utility>

using namespace world_detail;
using namespace hilbert_detail;

Ord const* hilbert::LUT_2d_to_1d = hilbert::make_LUT_2d_to_1d();

static void rot(long s, Len& x, Len& y, Ord rx, Ord ry)
{
    if (ry == 0)
    {
        if (rx == 1)
        {
            x = s-1 - x;
            y = s-1 - y;
        }

        Len tmp = y;
        y = x;
        x = tmp;
    }
}

static Ord from_2d_(Len x, Len y)
{
    Ord rx, ry, d = 0;

    for (Ord s = BLOCK_LEN/2; s > 0; s /= 2)
    {
        rx = (Ord(x) & s) > 0;
        ry = (Ord(y) & s) > 0;
        d += s * s * ((3 * rx) ^ ry);
        rot(s, x, y, rx, ry);
    }
    return d;
}

Ord const* hilbert::make_LUT_2d_to_1d()
{
    static Ord array[BLOCK_AREA];

    for (Len y = 0; y < BLOCK_LEN; y++)
        for (Len x = 0; x < BLOCK_LEN; x++)
            array[y*BLOCK_LEN + x] = from_2d_(x, y);

    return array;
}

Ord hilbert::lookup_2d_to_1d(Len2 xy)
{
    auto [x, y] = xy;
    return LUT_2d_to_1d[y * BLOCK_LEN + x];
}
