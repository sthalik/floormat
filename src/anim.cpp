#include "anim.hpp"
#include "compat/exception.hpp"
#include <cmath>

namespace floormat {

Vector2 anim_scale::scale_to_(Vector2ui image_size) const
{
    fm_soft_assert(image_size.product() > 0);
    Vector2 ret;
    switch (type)
    {
    default:
        fm_throw("invalid anim_scale_type '{}'"_cf, (unsigned)type);
    case anim_scale_type::invalid:
        fm_throw("anim_scale is invalid"_cf);
    case anim_scale_type::fixed:
        fm_soft_assert(f.width_or_height > 0);
        if (auto x = (float)image_size.x(), y = (float)image_size.y(), wh = (float)f.width_or_height; f.is_width)
            ret = { wh, wh * y/x };
        else
            ret = { wh * x/y, wh };
        break;
    case anim_scale_type::ratio:
        fm_soft_assert(r.f > 0 && r.f <= 1);
        ret = { image_size.x() * r.f, image_size.y() * r.f };
        break;
    }
    fm_soft_assert(ret.product() > 0);
    return ret;
}

Vector2ui anim_scale::scale_to(Vector2ui image_size) const
{
    Vector2 value = scale_to_(image_size);
    return { (unsigned)std::round(value[0]), (unsigned)std::round(value[1]) };
}

} // namespace floormat
