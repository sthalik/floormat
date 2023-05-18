#include "light.hpp"
#include "shaders/shader.hpp"
#include <cmath>

namespace floormat {

light_proto::light_proto()
{
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

Vector2 light::ordinal_offset(Vector2b) const { return {}; }
entity_type light::type() const noexcept { return entity_type::light; }
bool light::update(size_t, float) { return false; }
bool light::is_virtual() const { return true; }

float light::calc_intensity(float half_dist, light_falloff falloff)
{
    switch (falloff)
    {
    case light_falloff::linear: return 2 * half_dist;
    case light_falloff::quadratic: return std::sqrt(2 * half_dist);
    default: case light_falloff::constant: return 1;
    }
}

} // namespace floormat
