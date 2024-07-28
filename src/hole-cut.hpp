#pragma once
#include <mg/Vector2.h>
#include <Magnum/DimensionTraits.h>

namespace floormat {

template<typename T>
struct CutResult
{
    using Vec2 = VectorTypeFor<2, T>;
    struct bbox { Vec2 position; Vector2ub bbox_size; };
    struct rect { Vec2 min, max; };

    static CutResult cut(bbox input, bbox hole);
    static CutResult cut(Vec2 r0, Vec2 r1, Vec2 h0, Vec2 h1);

    std::array<rect, 8> array;
    uint8_t size = 0;
    bool found = false;
};

} // namespace floormat
