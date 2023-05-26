#include "light.hpp"
#include "shaders/shader.hpp"
#include "loader/loader.hpp"
#include "loader/vobj-info.hpp"
#include <cmath>

namespace floormat {

light_proto::light_proto()
{
    atlas = loader.vobj("light"_s).atlas;
    pass = pass_mode::pass;
    type = entity_type::light;
}

light_proto::light_proto(const light_proto&) = default;
light_proto& light_proto::operator=(const light_proto&) = default;
light_proto::~light_proto() noexcept = default;
bool light_proto::operator==(const light_proto&) const = default;

light::light(object_id id, struct chunk& c, const light_proto& proto) :
    entity{id, c, proto},
    half_dist{proto.half_dist},
    color{proto.color},
    falloff{proto.falloff}
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

entity_type light::type() const noexcept { return entity_type::light; }
bool light::update(size_t, float) { return false; }
bool light::is_dynamic() const { return true; }
bool light::is_virtual() const { return true; }

Vector2 light::intensity(Vector2 half_dist, light_falloff falloff)
{
    switch (falloff)
    {
    case light_falloff::linear: return 2 * half_dist;
    case light_falloff::quadratic: return Vector2{std::sqrt(2 * half_dist.x()), std::sqrt(2 * half_dist.y())};
    default: case light_falloff::constant: return Vector2{1, 1};
    }
}

Vector2 light::intensity() const
{
    return intensity(half_dist, falloff);
}

} // namespace floormat
