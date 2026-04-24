#pragma once
#include <array>
#include <mg/Vector2.h>
#include <mg/Range.h>
#include <mg/DimensionTraits.h>

namespace floormat {

template<typename T>
struct CutResult
{
    using Vec2 = VectorTypeFor<2, T>;

    static CutResult cut(Math::Range2D<T> input, Math::Range2D<T> hole);
    static CutResult cut(Vec2 r0, Vec2 r1, Vec2 h0, Vec2 h1);
    static CutResult cutʹ(Vec2 r0, Vec2 r1, Vec2 h0, Vec2 h1, uint8_t s);

    std::array<Math::Range2D<T>, 4> array;
    uint8_t s = (uint8_t)-1, size = 0;

    bool found() const;
};

} // namespace floormat
