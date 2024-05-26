#include "hole.hpp"
#include "compat/array-size.hpp"
#include "compat/iota.hpp"
#include "compat/map.hpp"
//#include <mg/Functions.h>

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wswitch-default"
//#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

namespace floormat {
namespace {

using bbox = cut_rectangle_result::bbox;

enum shift : uint8_t { __ = 0, x0 = 1 << 0, x1 = 1 << 1, y0 = 1 << 2, y1 = 1 << 3, };
enum class location : uint8_t { R0, R1, H0, H1, };

struct coords
{
    location x0 : 2;
    location x1 : 2;
    location y0 : 2;
    location y1 : 2;
};

struct element
{
    uint8_t size;
    std::array<coords, 8> array;
};

constexpr element make_element(uint8_t s)
{
    switch (s)
    {
    using enum location;
    case x0|x1|y0|y1: return element{1, {{ // 9.1
        {R0, R1, R0, R1},
    }}};
    case __|__|__|__: return element{8, {{ // 14.1
        {R0, H0, R0, H0},
        {H0, H1, R0, H0},
        {H1, R1, R0, H0},
        {R0, H0, H0, H1},
        {H1, R1, H0, H1},
        {R0, H0, H1, R1},
        {H0, H1, H0, R1},
        {H1, R1, H1, R1},
    }}};

    case x0|x1|__|__: return element{2, {{ // 13.1
        {R0, R1, R0, H0},
        {R0, R1, H1, R1},
    }}};
    case __|__|y0|y1: return element{2, {{ // 13.2
        {R0, H0, H1, R1},
        {H1, R1, R0, R1},
    }}};

    case x0|x1|y0|__: return element{1, {{ // 12.1
        {R0, R1, H1, R1},
    }}};
    case x0|x1|__|y1: return element{1, {{ // 12.2
        {R0, R1, R0, H0},
    }}};
    case x0|__|y0|y1: return element{1, {{ // 12.3
        {H1, R1, R0, R1},
    }}};
    case __|x1|y0|y1: return element{1, {{ // 12.4
        {R0, H0, R0, R1},
    }}};

    case x0|__|__|__: return element{3, {{ // 10.1
        {R0, R1, R0, H0},
        {H1, R1, H0, H1},
        {R0, R1, H1, R1},
    }}};
    case __|x1|__|__: return element{3, {{ // 10.2
        {R0, R1, R0, H0},
        {R0, H0, H0, H1},
        {R0, R1, H1, R1},
    }}};
    case __|__|y0|__: return element{3, {{ // 10.3
        {R0, H0, R0, R1},
        {H0, H1, H0, R1},
        {H1, R1, R0, R1},
    }}};
    case __|__|__|y1: return element{3, {{ // 10.4
        {R0, H0, R0, R1},
        {H0, H1, R0, H0},
        {H1, R1, R0, R1},
    }}};

    case x0|__|y0|__: return element{2, {{ // 11.1
        {H1, R1, R0, H1},
        {R0, R1, H1, R1},
    }}};
    case __|x1|y0|__: return element{2, {{ // 11.2
        {R0, H0, R0, H1},
        {R0, R1, H1, R1},
    }}};
    case x0|__|__|y1: return element{2, {{ // 11.3
        {R0, R1, R0, H0},
        {H1, R1, H0, R1},
    }}};
    case __|x1|__|y1: return element{2, {{ // 11.4
        {R0, R1, R0, H0},
        {R0, H0, H0, H1},
    }}};
    }
    fm_assert(false);
}

constexpr auto elements = map(make_element, iota_array<uint8_t, 16>);

constexpr cut_rectangle_result cut_rectangleʹ(bbox input, bbox hole)
{
    auto ihalf = Vector2i{input.bbox_size/2};
    auto r0 = input.position - ihalf;
    auto r1 = input.position + Vector2i{input.bbox_size} - ihalf;

    auto hhalf = Vector2i{hole.bbox_size/2};
    auto h0 = hole.position - hhalf;
    auto h1 = hole.position + Vector2i{hole.bbox_size} - hhalf;

    {
        bool iempty = Vector2ui{input.bbox_size}.product() == 0;
        bool hempty = Vector2ui{hole.bbox_size}.product() == 0;
        bool empty_before_x = h1.x() <= r0.x();
        bool empty_after_x  = h0.x() >= r1.x();
        bool empty_before_y = h1.y() <= r0.y();
        bool empty_after_y  = h0.y() >= r1.y();

        if (iempty | hempty | empty_before_x | empty_after_x | empty_before_y | empty_after_y) [[unlikely]]
            return { 0, {} };
    }

    const bool sx = h0.x() <= r0.x();
    const bool ex = h1.x() >= r1.x();
    const bool sy = h0.y() <= r0.y();
    const bool ey = h1.y() >= r1.y();

    auto val = uint8_t(sx << 0 | ex << 1 | sy << 2 | ey << 3);
    CORRADE_ASSUME(val < 16);
    const auto elt = elements[val];
    cut_rectangle_result res = {
        .size = elt.size,
        .array = {},
    };

    const Int xs[4] = { r0.x(), r1.x(), h0.x(), h1.x(), };
    const Int ys[4] = { r0.y(), r1.y(), h0.y(), h1.y(), };

    for (auto i = 0uz; i < elt.size; i++)
    {
        const auto e = elt.array[i];

        const auto x0 = xs[(uint8_t)e.x0];
        const auto x1 = xs[(uint8_t)e.x1];
        const auto y0 = ys[(uint8_t)e.y0];
        const auto y1 = ys[(uint8_t)e.y1];

        res.array[i] = { {x0, y0}, {x1, y1}, };
    }

    return res;
}

} // namespace

cut_rectangle_result cut_rectangle(bbox input, bbox hole)
{
    return cut_rectangleʹ(input, hole);
}

} // namespace floormat
