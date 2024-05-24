#include "app.hpp"
#include "src/hole.hpp"
#include "compat/array-size.hpp"
#include "compat/map.hpp"
#include "compat/iota.hpp"

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wswitch-default"
//#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

namespace floormat::Hole {
//namespace {

using bbox = cut_rectangle_result::bbox;

enum class type : uint8_t
{
    // see `doc/cut-rectangle-hole'
    _9 , // all
    _10, // side-part
    _11, // corner
    _12, // full-side
    _13, // middle
    _14, // center
};

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
    uint8_t count;
    std::array<coords, 8> array;
};

constexpr element make_element(uint8_t s)
{
    switch (s)
    {
    using enum type;
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
        {R0, H0, H0, R1},
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

    }}};
    case __|x1|y0|__: return element{2, {{ // 11.1

    }}};
    case x0|__|__|y1: return element{2, {{ // 11.1

    }}};
    case __|x1|__|y1: return element{2, {{ // 11.1

    }}};
    }
    fm_assert(false);
}
constexpr auto elements = map(make_element, iota_array<uint8_t, 16>);
static_assert(sizeof(elements) == 9 * 16);

constexpr cut_rectangle_result cut(bbox input, bbox hole)
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
            return { 0, 0, {} };
    }

    const bool sx = h0.x() <= r0.x();
    const bool ex = h1.x() >= r1.x();
    const bool sy = h0.y() <= r0.y();
    const bool ey = h1.y() >= r1.y();

    auto val = uint8_t(sx << 0 | ex << 1 | sy << 2 | ey << 3);
    CORRADE_ASSUME(val < 16);

    //static_assert(array_size(starts) == 16);
    fm_assert(false);
    std::unreachable();

    //return -1;
}

template<Int x, Int y>
void test1()
{
    static constexpr auto vec = Vector2i{x, y};
    constexpr auto rect = bbox{{}, {50, 50}};
    constexpr auto cutʹ = [](bbox rect, bbox hole) {
        auto rectʹ = bbox { rect.position + vec, rect.bbox_size };
        auto holeʹ = bbox { hole.position + vec, hole.bbox_size };
        return cut(rectʹ, holeʹ).code;
    };
#if 1
    fm_assert_not_equal(0, cutʹ(rect, {{ 49,   0}, {50, 50}}));
    fm_assert_not_equal(0, cutʹ(rect, {{  0,  49}, {50, 50}}));
    fm_assert_not_equal(0, cutʹ(rect, {{ 49,  49}, {50, 50}}));
#endif
#if 1
    fm_assert_not_equal(0, cutʹ(rect, {{-49,   0}, {50, 50}}));
    fm_assert_not_equal(0, cutʹ(rect, {{  0, -49}, {50, 50}}));
    fm_assert_not_equal(0, cutʹ(rect, {{ 49, -49}, {50, 50}}));
#endif
#if 1
    fm_assert_equal(0, cutʹ(rect, {{50,  0}, {50, 50}}));
    fm_assert_equal(0, cutʹ(rect, {{ 0, 50}, {50, 50}}));
    fm_assert_equal(0, cutʹ(rect, {{50, 50}, {50, 50}}));
#endif
#if 1
    fm_assert_equal(9, cutʹ(rect, {{ 9,  9}, {70, 70}}));
    fm_assert_equal(9, cutʹ(rect, {{10, 10}, {70, 70}}));
#endif
#if 1
    fm_assert_equal(12, cutʹ(rect, {{1, 0}, {50, 50}}));
    fm_assert_equal(12, cutʹ(rect, {{0, 1}, {50, 50}}));
    fm_assert_equal(11, cutʹ(rect, {{1, 1}, {50, 50}}));
#endif
#if 1
    // todo! coverage
#endif
}

//} // namespace
} // namespace floormat::Hole

namespace floormat {

void Test::test_hole()
{
    Hole::test1<   0,     0 >();
    Hole::test1<  110,  105 >();
    Hole::test1<   15,  110 >();
    Hole::test1< - 15, -110 >();
    Hole::test1< -110,  -15 >();
}

} // namespace floormat
