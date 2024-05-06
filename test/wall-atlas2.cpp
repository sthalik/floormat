#include "app.hpp"
#include "compat/assert.hpp"
#include "src/tile-constants.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include "loader/wall-cell.hpp"

namespace floormat {

namespace {

void test_loading()
{
    { auto walls = loader.wall_atlas_list();
      fm_assert(!walls.isEmpty());
      fm_assert(loader.wall_atlas("test1"_s));
      fm_assert(loader.wall_atlas(loader.INVALID, loader_policy::ignore));
      fm_assert(loader.wall_atlas("test1"_s) == loader.wall_atlas("test1"_s));
      fm_assert(loader.wall_atlas("test1"_s) != loader.wall_atlas(loader.INVALID, loader_policy::ignore));
    }
    for (const auto& x : loader.wall_atlas_list())
    {
        if (x.name != loader.INVALID)
        {
            (void)loader.wall_atlas(x.name);
            fm_assert(x.atlas);
            fm_assert(x.atlas == loader.wall_atlas(x.name));
        }
        else
        {
            fm_assert(loader.invalid_wall_atlas().atlas);
            fm_assert(x.atlas == loader.invalid_wall_atlas().atlas);
        }
    }
}

void test_empty_wall()
{
    constexpr auto wall_size = Vector2ui(Vector2i{iTILE_SIZE.x(), iTILE_SIZE.z()});
    using enum Wall::Direction_;
    const auto& wall = *loader.wall_atlas("empty"_s);

#define assert_for_group(dir, g, x)                 \
    do {                                            \
        fm_assert(!g.is_defined);                   \
        fm_assert(g == Wall::Group{});              \
        fm_assert(g.count == 0);                    \
        fm_assert(g.index == (uint32_t)-1);         \
        fm_assert(g == Wall::Group{});              \
        fm_assert(!wall.group(dir, x));             \
    } while (false)

    fm_assert(wall_atlas::expected_size(wall.info().depth, Wall::Group_::wall) == wall_size);
    fm_assert(wall.depth() == 8);
    fm_assert(wall.direction_count() == 1);
    fm_assert(wall.raw_frame_array().size() == 1);
    fm_assert(wall.direction(N) != nullptr);
    const auto& n = *wall.direction(N);
    fm_assert(wall.direction(W) == nullptr);
    fm_assert(&wall.calc_direction(N) == wall.direction(N));
    fm_assert(&wall.calc_direction(W) == wall.direction(N));
    assert_for_group(N, n.side, Wall::Group_::side);
    assert_for_group(N, n.top, Wall::Group_::top);
    assert_for_group(N, n.corner, Wall::Group_::corner);
    fm_assert(n.wall.is_defined);
    fm_assert(n.wall != Wall::Group{});
    fm_assert(!n.wall.mirrored);
    fm_assert(n.wall.count > 0);
    fm_assert(n.wall.index != (uint32_t)-1);
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

    auto& wall = *loader.wall_atlas(name);
    fm_assert(wall.name() == name);
    fm_assert(wall.info().depth == 20);
    fm_assert(wall.raw_frame_array().size() >= 3);
    fm_assert(!wall.direction(W));
    fm_assert(wall.direction(N));
    fm_assert(&wall.calc_direction(W) == wall.direction(N));
    fm_assert(&wall.calc_direction(N) == wall.direction(N));
    fm_assert(wall.frames(N, Wall::Group_::wall).size() >= 3);
    fm_assert(wall.group(N, Wall::Group_::corner)->count > 0);
    fm_assert(wall.group(N, Wall::Group_::side)->count > 0);
    fm_assert(wall.group(N, Wall::Group_::top)->count > 0);
    fm_assert(wall.group(N, Wall::Group_::wall)->count > 1);
    fm_assert(wall.group(N, Wall::Group_::corner)->is_defined);
    fm_assert(wall.frames(N, Wall::Group_::wall)[0].size == Vector2ui(Vector2i{iTILE_SIZE.x(), iTILE_SIZE.z()}));
    fm_assert(&wall.calc_direction(N) == wall.direction(N));
    fm_assert(&wall.calc_direction(W) == wall.direction(N));
}

} // namespace

void Test::test_wall_atlas2()
{
    test_empty_wall();
    test_loading();
    test_concrete_wall();
}

// todo add test for wall-tileset-tool for making sure it generates the correct image pixels out of placeholder input

} // namespace floormat
