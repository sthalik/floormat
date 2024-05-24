#include "hole.hpp"
//#include <mg/Functions.h>

namespace floormat {

using bbox = cut_rectangle_result::bbox;

namespace {


} // namespace

cut_rectangle_result cut_rectangle(cut_rectangle_result::bbox input, cut_rectangle_result::bbox hole)
{
    const auto ihalf  = Vector2i{input.bbox_size/2};
    const auto istart = input.position - ihalf;
    const auto iend   = input.position + Vector2i{input.bbox_size} - ihalf;
    const auto hhalf  = Vector2i{hole.bbox_size/2};
    const auto hstart = hole.position - hhalf;
    const auto hend   = hole.position + Vector2i{hole.bbox_size} - hhalf;

    //std::atomic_signal_fence(std::memory_order::relaxed); // compiler-only barrier

    {
        bool iempty = Vector2ui{input.bbox_size}.product() == 0;
        bool hempty = Vector2ui{hole.bbox_size}.product() == 0;
        bool empty_before_x  = hend.x() <= istart.x();
        bool empty_before_y  = hend.y() <= istart.y();
        bool empty_after_x   = hstart.x() >= iend.x();
        bool empty_after_y   = hstart.y() >= iend.y();
        if (iempty | hempty | empty_before_x | empty_before_y | empty_after_x | empty_after_y)
            return {};
    }

    const bool sx = hstart.x() <= istart.x();
    const bool ex = hend.x()   >= iend.x();
    const bool sy = hstart.y() <= istart.y();
    const bool ey = hend.y()   >= iend.y();

    enum : uint8_t { SX, EX, SY, EY };
    enum : uint8_t { startx = 1 << SX, endx = 1 << EX, starty = 1 << SY, endy = 1 << EY };

    constexpr auto mask = uint8_t(startx | endx | starty | endy);
    auto val = uint8_t(sx << SX | ex << EX | sy << SY | ey << EY);
    CORRADE_ASSUME((val & mask) == val);
    (void)val;

    return {};
}


} // namespace floormat
