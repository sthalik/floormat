#include "hole.hpp"
#include "compat/array-size.hpp"
#include "compat/iota.hpp"
#include "compat/map.hpp"
//#include <mg/Functions.h>

#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wswitch-default"
//#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#ifdef __clang__
#pragma clang diagnostic ignored "-Wbitwise-instead-of-logical"
#endif

namespace floormat {
namespace {

template<typename T> using Cr = CutResult<T>;
template<typename T> using bbox = typename Cr<T>::bbox;
template<typename T> using rect = typename Cr<T>::rect;

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

template<typename T> using Vec2ʹ = VectorTypeFor<2, T>;

constexpr element make_element(uint8_t s)
{
    // NOLINTBEGIN(*-simplify, *-redundant-expression)
    // ReSharper disable CppIdenticalOperandsInBinaryExpression
    switch (s)
    {
    using enum location;
    case x0|x1|y0|y1: return element{0, {{ // 9.1
    }}};
    case __|__|__|__: return element{8, {{ // 14.1
        {R0, H0, R0, H0},
        {H0, H1, R0, H0},
        {H1, R1, R0, H0},
        {R0, H0, H0, H1},
        {H1, R1, H0, H1},
        {R0, H0, H1, R1},
        {H0, H1, H1, R1},
        {H1, R1, H1, R1},
    }}};

    case x0|x1|__|__: return element{2, {{ // 13.1
        {R0, R1, R0, H0},
        {R0, R1, H1, R1},
    }}};
    case __|__|y0|y1: return element{2, {{ // 13.2
        {R0, H0, R0, R1},
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
        {H0, H1, H1, R1},
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
        {R0, H0, H0, R1},
    }}};
    }
    // ReSharper restore CppIdenticalOperandsInBinaryExpression
    // NOLINTEND(*-simplify, *-redundant-expression)
    fm_assert(false);
}

constexpr auto elements = map(make_element, iota_array<uint8_t, 16>);

template<typename T>
constexpr auto get_value_from_coord(Vec2ʹ<T> r0, Vec2ʹ<T> r1, Vec2ʹ<T> h0, Vec2ʹ<T> h1, coords c)
{
    const auto xs = std::array{ r0.x(), r1.x(), h0.x(), h1.x(), };
    const auto ys = std::array{ r0.y(), r1.y(), h0.y(), h1.y(), };
    const auto x0 = xs[(uint8_t)c.x0];
    const auto x1 = xs[(uint8_t)c.x1];
    const auto y0 = ys[(uint8_t)c.y0];
    const auto y1 = ys[(uint8_t)c.y1];
    return rect<T>{ {x0, y0}, {x1, y1} };
}

template<typename T>
[[nodiscard]]
constexpr bool check_empty(Vec2ʹ<T> r0, Vec2ʹ<T> r1, Vec2ʹ<T> h0, Vec2ʹ<T> h1)
{
    bool iempty = r0.x() == r1.x() | r0.y() == r1.y();
    bool hempty = h0.x() == h1.x() | h0.y() == h1.y();
    bool empty_before_x = h1.x() <= r0.x();
    bool empty_after_x  = h0.x() >= r1.x();
    bool empty_before_y = h1.y() <= r0.y();
    bool empty_after_y  = h0.y() >= r1.y();
    return iempty | hempty | empty_before_x | empty_after_x | empty_before_y | empty_after_y;
}

template<typename T>
constexpr Cr<T> cut_rectangle(Vec2ʹ<T> r0, Vec2ʹ<T> r1, Vec2ʹ<T> h0, Vec2ʹ<T> h1)
{
    if (check_empty<T>(r0, r1, h0, h1))
        return {
            .array = {{ { r0, r1 }, }},
            .size = 1,
            .found = false,
        };

    const bool sx = h0.x() <= r0.x();
    const bool ex = h1.x() >= r1.x();
    const bool sy = h0.y() <= r0.y();
    const bool ey = h1.y() >= r1.y();

    auto val = uint8_t(sx << 0 | ex << 1 | sy << 2 | ey << 3);
    CORRADE_ASSUME(val < 16);
    const auto elt = elements[val];
    const auto sz = elt.size;
    Cr<T> res = {
        .array = {},
        .size = sz,
        .found = true,
    };

    for (auto i = 0u; i < 8; i++)
        res.array[i] = get_value_from_coord<T>(r0, r1, h0, h1, elt.array[i]);

    return res;
}

template<typename T>
constexpr Cr<T> cut_rectangle(bbox<T> input, bbox<T> hole)
{
    using Vec2 = Vec2ʹ<T>;

    auto ihalf = Vec2{input.bbox_size/2};
    auto r0 = input.position - ihalf;
    auto r1 = input.position + Vec2{input.bbox_size} - ihalf;

    auto hhalf = Vec2{hole.bbox_size/2};
    auto h0 = hole.position - hhalf;
    auto h1 = hole.position + Vec2{hole.bbox_size} - hhalf;

    return cut_rectangle<T>(r0, r1, h0, h1);
}

} // namespace

template<typename T> Cr<T> CutResult<T>::cut(bbox input, bbox hole) { return cut_rectangle<T>(input, hole); }
template<typename T> Cr<T> CutResult<T>::cut(Vec2 r0, Vec2 r1, Vec2 h0, Vec2 h1) { return cut_rectangle<T>(r0, r1, h0, h1); }

template struct CutResult<Int>;
template struct CutResult<float>;

} // namespace floormat
