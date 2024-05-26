#include "hole.hpp"
#include "chunk.hpp"
#include "tile-constants.hpp"
#include "shaders/shader.hpp"

namespace floormat {
namespace {

} // namespace

hole::hole(object_id id, floormat::chunk& c, const hole_proto& proto):
    object{id, c, proto}
{
}

hole::~hole() noexcept
{
    c->mark_ground_modified();
    c->mark_walls_modified();
    c->mark_passability_modified();
}

void hole::update(const std::shared_ptr<object>& ptr, size_t& i, const Ns& dt)
{
}

hole::operator hole_proto() const
{
    hole_proto ret;
    static_cast<object_proto&>(ret) = object_proto(*this);
    ret.max_distance = max_distance;
    ret.color = color;
    ret.falloff = falloff;
    ret.enabled = enabled;
    return ret;
}

float hole::depth_offset() const
{
    constexpr auto ret = 4 / tile_shader::depth_tile_size;
    return ret;
}

Vector2 hole::ordinal_offset(Vector2b) const
{
    constexpr auto ret = Vector2(TILE_COUNT, TILE_COUNT) * TILE_SIZE2;
    return ret;
}

object_type hole::type() const noexcept { return object_type::hole; }
bool hole::is_virtual() const { return true; }
bool hole::is_dynamic() const { return false; }

} // namespace floormat
