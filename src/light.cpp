#include "light.hpp"
#include "nanosecond.hpp"
#include "tile-constants.hpp"
#include "shaders/shader.hpp"
#include "loader/loader.hpp"
#include "loader/vobj-cell.hpp"
#include <cmath>

namespace floormat {

light_proto::light_proto()
{
    atlas = loader.vobj("light"_s).atlas;
    pass = pass_mode::pass;
    type = object_type::light;
}

light_proto::~light_proto() noexcept = default;
light_proto::light_proto(const light_proto&) = default;
light_proto& light_proto::operator=(const light_proto&) = default;
light_proto::light_proto(light_proto&&) noexcept = default;
light_proto& light_proto::operator=(light_proto&&) noexcept = default;

bool light_proto::operator==(const object_proto& oʹ) const
{
    if (type != oʹ.type)
        return false;

    if (!object_proto::operator==(oʹ))
        return false;

    const auto& o = static_cast<const light_proto&>(oʹ);

    return Math::abs(max_distance - o.max_distance) < 1e-8f &&
           color == o.color &&
           falloff == o.falloff &&
           enabled == o.enabled;
}

light::light(object_id id, class chunk& c, const light_proto& proto) :
    object{id, c, proto},
    max_distance{proto.max_distance},
    color{proto.color},
    falloff{proto.falloff},
    enabled{proto.enabled}
{
}

float light::depth_offset() const
{
    constexpr auto ret = 4 / tile_shader::depth_tile_size;
    return ret;
}

Vector2 light::ordinal_offset(Vector2b) const
{
    constexpr auto ret = Vector2(TILE_COUNT, TILE_COUNT) * TILE_SIZE2;
    return ret;
}

light::operator light_proto() const
{
    light_proto ret;
    static_cast<object_proto&>(ret) = object_proto(*this);
    ret.max_distance = max_distance;
    ret.color = color;
    ret.falloff = falloff;
    ret.enabled = enabled;
    return ret;
}

object_type light::type() const noexcept { return object_type::light; }
void light::update(const bptr<object>&, size_t&, const Ns&) {}
bool light::is_dynamic() const { return true; }
bool light::is_virtual() const { return true; }

} // namespace floormat
