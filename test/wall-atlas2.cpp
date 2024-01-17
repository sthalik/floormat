#include "app.hpp"
#include "compat/assert.hpp"
#include "src/tile-defs.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include "loader/wall-info.hpp"

namespace floormat {

namespace {

void test_empty_wall()
{
    using enum Wall::Direction_;

    const auto& wall = *loader.wall_atlas("empty"_s);
    constexpr auto wall_size = Vector2ui(Vector2i{iTILE_SIZE.x(), iTILE_SIZE.z()});
    fm_assert(wall_atlas::expected_size(wall.info().depth, Wall::Group_::wall) == wall_size);
    fm_assert(wall.depth() == 8);
    fm_assert(wall.direction_count() == 1);
    fm_assert(wall.raw_frame_array().size() == 1);
    fm_assert(wall.direction(N) != nullptr);
    const auto& n = *wall.direction(N);
    fm_assert(wall.direction(W) == nullptr);
    fm_assert(&wall.calc_direction(N) == wall.direction(N));
    fm_assert(&wall.calc_direction(W) == wall.direction(N));
    fm_assert(n.side == Wall::Group{});
    fm_assert(n.top == Wall::Group{});
    fm_assert(n.corner == Wall::Group{});
    fm_assert(!n.wall.mirrored);
    fm_assert(n.wall.pixel_size == wall_size);
    fm_assert(wall.raw_frame_array()[0].offset.isZero());
    fm_assert(wall.raw_frame_array()[0].size == wall_atlas::expected_size(wall.info().depth, Wall::Group_::wall));
    fm_assert(wall.info().name == "empty"_s);
    fm_assert(wall.info().passability == pass_mode::blocked);
}

void test_concrete_wall()
{
    using enum Wall::Direction_;
    constexpr auto name = "concrete1"_s;

    auto& wall = *loader.wall_atlas(name, false);
    fm_assert(wall.name() == name);
    fm_assert(wall.info().depth == 20);
    fm_assert(wall.raw_frame_array().size() >= 3);
    fm_assert(!wall.direction(W));
    fm_assert(wall.direction(N));
    fm_assert(&wall.calc_direction(W) == wall.direction(N));
    fm_assert(&wall.calc_direction(N) == wall.direction(N));
    fm_assert(wall.frames(N, Wall::Group_::wall).size() >= 3);
    fm_assert(wall.group(N, Wall::Group_::top)->is_defined);
    fm_assert(wall.frames(N, Wall::Group_::wall)[0].size == Vector2ui(Vector2i{iTILE_SIZE.x(), iTILE_SIZE.z()}));
    fm_assert(&wall.calc_direction(N) == wall.direction(N));
    fm_assert(&wall.calc_direction(W) == wall.direction(N));
}

} // namespace

void test_app::test_wall_atlas2()
{
    test_empty_wall();
    test_concrete_wall();
}

// todo add test for wall-tileset-tool for making sure it generates the correct image pixels out of placeholder input

} // namespace floormat
