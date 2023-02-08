#include "anim.hpp"
#include "compat/exception.hpp"
#include <cmath>

namespace floormat {

Vector2ui anim_scale::scale_to(Vector2ui image_size) const
{
    fm_soft_assert(image_size.product() > 0);
    Vector2ui ret;
    switch (type)
    {
    default:
        fm_throw("invalid anim_scale_type '{}'"_cf, (unsigned)type);
    case anim_scale_type::invalid:
        fm_throw("anim_scale is invalid"_cf);
    case anim_scale_type::fixed:
        fm_soft_assert(f.width_or_height > 0);
        if (f.is_width)
            ret = { f.width_or_height, (unsigned)std::round((float)f.width_or_height * (float)image_size.y()/(float)image_size.x()) };
        else
            ret = { (unsigned)std::round((float)f.width_or_height * (float)image_size.x()/(float)image_size.y()), f.width_or_height };
        break;
    case anim_scale_type::ratio:
        fm_soft_assert(r.f > 0 && r.f <= 1);
        ret = { (unsigned)std::round(image_size.x() * r.f), (unsigned)std::round(image_size.y() * r.f) };
        break;
    }
    fm_soft_assert(ret.product() > 0);
    return ret;
}

} // namespace floormat
