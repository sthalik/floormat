#include "app.hpp"
#include "src/intra-coord.inl"
#include "src/point.hpp"

namespace floormat {

namespace {

using wrapping_coord = basic_intra_coord<intra_coord::Wrapping>;

constexpr auto last = chunk_size<Vector2i> - Vector2i{1};

static_assert(std::is_trivially_copyable_v<intra_coord> && std::is_trivially_copyable_v<wrapping_coord>);
static_assert(sizeof(intra_coord) == sizeof(Vector2i) && sizeof(wrapping_coord) == sizeof(Vector2i));
static_assert(std::is_same_v<decltype(intra_coord{} + Vector2i{}), intra_coord>);
static_assert(std::is_same_v<decltype(wrapping_coord{} + Vector2i{}), point>);
static_assert(std::is_same_v<decltype(intra_coord{} - intra_coord{}), Vector2i>);

static_assert(Vector2i(intra_coord{{0, 0}, -half_tile<Vector2b>}) == Vector2i{0});
static_assert(Vector2i(intra_coord{{15, 15}, half_tile<Vector2b> - Vector2b{1}}) == last);
static_assert(intra_coord{}.center_shifted() == -half_tile<Vector2i>);
static_assert(intra_coord::from_center_shifted({}) == intra_coord{{0, 0}, {}});
static_assert(intra_coord::is_in_range({}) && intra_coord::is_in_range(last));
static_assert(!intra_coord::is_in_range({-1, 0}) && !intra_coord::is_in_range({0, chunk_size<int>}));
static_assert(intra_coord{last}.local() == local_coords{15, 15});
static_assert(intra_coord{last}.offset() == half_tile<Vector2b> - Vector2b{1});
static_assert(intra_coord{{3, 4}, {5, -6}} - intra_coord{{1, 1}, {}} == Vector2i{2, 3}*tile_size_xy + Vector2i{5, -6});
static_assert(point{{-3, 7, 1}, intra_coord{last}} == point{{-3, 7, 1}, {15, 15}, half_tile<Vector2b> - Vector2b{1}});
static_assert(point{{-3, 7, 1}, intra_coord{last}}.intra() == intra_coord{last});
static_assert(point{{-3, 7, 1}, intra_coord{last}}.intra<intra_coord::Wrapping>() == wrapping_coord{last});

constexpr bool test_checking()
{
    auto a = intra_coord{{3, 4}, {}};
    a += Vector2i{10, -20};
    fm_assert(a.center_shifted() == Vector2i{3, 4}*tile_size_xy + Vector2i{10, -20});
    a -= Vector2i{10, -20};
    fm_assert(a == intra_coord{{3, 4}, {}});
    a = last;
    fm_assert(Vector2i(a) == last);

    fm_assert(Vector2i(intra_coord{Vector2i{5, 6}} + Vector2i{1, 2}) == Vector2i{6, 8});
    fm_assert(Vector2i(Vector2i{1, 2} + intra_coord{Vector2i{5, 6}}) == Vector2i{6, 8});
    fm_assert(Vector2i(intra_coord{Vector2i{5, 6}} - Vector2i{1, 2}) == Vector2i{4, 4});

    const intra_coord c = wrapping_coord{last};
    fm_assert(Vector2i(c) == last);
    return true;
}

constexpr bool test_wrapping()
{
    auto w = wrapping_coord{Vector2i{1020, 5}};
    fm_assert((w += Vector2i{10, 0}) == Vector2i{1, 0});
    fm_assert(Vector2i(w) == Vector2i{6, 5});
    fm_assert((w -= Vector2i{7, 6}) == Vector2i{-1, -1});
    fm_assert(Vector2i(w) == last);
    fm_assert((w += Vector2i{2, -2}*chunk_size<int>) == Vector2i{2, -2});
    fm_assert(Vector2i(w) == last);

    auto o = wrapping_coord{Vector2i{2000, -5}};
    fm_assert((o += Vector2i{}) == Vector2i{1, -1});
    fm_assert(Vector2i(o) == Vector2i{976, 1019});

    fm_assert(wrapping_coord{Vector2i{1020, 5}} + Vector2i{10, 0} == point{{1, 0, 0}, {0, 0}, {-26, -27}});
    fm_assert(Vector2i{10, 0} + wrapping_coord{Vector2i{1020, 5}} == point{{1, 0, 0}, {0, 0}, {-26, -27}});
    fm_assert(wrapping_coord{Vector2i{5, 5}} - Vector2i{10, 0} == point{{-1, 0, 0}, {15, 0}, {27, -27}});
    return true;
}

static_assert(test_checking());
static_assert(test_wrapping());

void test_round_trip()
{
    constexpr chunk_coords_ ch{-3, 7, 1};
    constexpr int8_t offsets[] = { -half_tile<int8_t>, 0, half_tile<int8_t> - 1 };
    for (uint32_t i = 0; i < TILE_COUNT; i++)
        for (auto ox : offsets)
            for (auto oy : offsets)
            {
                const auto pt = point{ch, local_coords{i}, Vector2b{ox, oy}};
                const auto ic = intra_coord{pt};
                fm_assert(intra_coord::is_in_range(Vector2i(ic)));
                fm_assert(ic.center_shifted() == Vector2i(pt.local())*tile_size_xy + Vector2i(pt.offset()));
                fm_assert_equal(pt, ic.to_point(ch));
                fm_assert_equal(pt, wrapping_coord{pt}.to_point(ch));
                fm_assert_equal(pt, point{ch, ic});
                fm_assert(pt.intra() == ic);
                fm_assert_equal(pt, pt.intra<intra_coord::Wrapping>().to_point(ch));
            }
}

} // namespace

void Test::test_intra_coord()
{
    test_round_trip();
}

} // namespace floormat
