#include "hole.hpp"
#include "chunk.hpp"
#include "loader/loader.hpp"
#include "loader/vobj-cell.hpp"
#include "shaders/shader.hpp"
#include "tile-constants.hpp"
#include "compat/non-const.hpp"

namespace floormat {
namespace {

} // namespace

hole_proto::~hole_proto() noexcept = default;
hole_proto::hole_proto(const hole_proto&) = default;
hole_proto& hole_proto::operator=(const hole_proto&) = default;
hole_proto::hole_proto(hole_proto&&) noexcept = default;
hole_proto& hole_proto::operator=(hole_proto&&) noexcept = default;

bool hole_proto::flags::operator==(const struct flags&) const = default;

bool hole_proto::operator==(const object_proto& oʹ) const
{
    if (type != oʹ.type)
        return false;

    if (!object_proto::operator==(oʹ))
        return false;

    const auto& o = static_cast<const hole_proto&>(oʹ);
    return height == o.height && z_offset == o.z_offset && flags == o.flags;
}

hole_proto::hole_proto()
{
    atlas = loader.vobj("hole"_s).atlas;
    pass = pass_mode::pass;
    type = object_type::hole;
}

hole::hole(object_id id, floormat::chunk& c, const hole_proto& proto):
    object{id, c, proto}, height{proto.height}, flags{proto.flags}
{
}

hole::~hole() noexcept
{
    if (c->is_teardown()) [[unlikely]]
        return;
    mark_chunk_modified();
}

void hole::update(const std::shared_ptr<object>&, size_t&, const Ns&) {}

hole::operator hole_proto() const
{
    hole_proto ret;
    static_cast<object_proto&>(ret) = object_proto(*this);
    ret.height = height;
    ret.flags = flags;
    return ret;
}

void hole::mark_chunk_modified()
{
    //c->mark_ground_modified(); // todo!
    //c->mark_walls_modified();  // todo!
    c->mark_passability_modified();
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

void hole::set_height(uint8_t heightʹ)
{
    if (height != heightʹ)
    {
        const_cast<uint8_t&>(height) = heightʹ;
        mark_chunk_modified();
    }
}

void hole::set_z_offset(uint8_t z)
{
    if (z_offset != z)
    {
        const_cast<uint8_t&>(z_offset) = z;
        mark_chunk_modified();
    }
}


void hole::set_enabled(bool on_render, bool on_physics)
{
    non_const(flags).on_render = on_render;

    if (flags.on_physics != on_physics)
    {
        non_const(flags).on_physics = on_physics;
        mark_chunk_modified();
    }
}

object_type hole::type() const noexcept { return object_type::hole; }
bool hole::is_virtual() const { return true; }
bool hole::is_dynamic() const { return true; }

} // namespace floormat
