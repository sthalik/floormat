#include "app.hpp"
#include "main/clickable.hpp"
#include "shaders/shader.hpp"
#include "src/anim-atlas.hpp"
#include "src/light.hpp"
#include "src/object.hpp"
#include "src/point.inl"
#include "src/world.hpp"
#include "compat/borrowed-ptr.inl"
#include <initializer_list>

namespace floormat {

namespace {

constexpr Vector2i win{641, 479}, shift{-37, 53};

Vector2i camera_centering(const object& e)
{
    return win/2 - tile_shader::point_to_pixelʹ(Vector3i(e.position()), win, {}) + shift;
}

void test_light_icon()
{
    world w;
    constexpr chunk_coords_ ch{0, 0, 0};
    const auto L = w.make_object<light>(w.make_id(), global_coords{ch, local_coords{8, 8}}, light_proto{});
    const auto camera = camera_centering(*L);
    const auto cl = clickable_for_object(*L, win, camera);
    fm_assert(cl);
    fm_assert_equal(L->id, cl->id);

    const auto size = Vector2i(L->atlas->frame(L->r, L->frame).size);
    const auto center = tile_shader::point_to_pixelʹ(Vector3i(L->position()), win, camera);
    fm_assert_equal(size, cl->dest.size());
    fm_assert_equal(center, cl->dest.center());

    const auto min = cl->dest.min(), max = cl->dest.max();
    for (const auto p : {center, min, max - Vector2i{1, 1},
                         Vector2i{min.x(), max.y() - 1}, Vector2i{max.x() - 1, min.y()}})
        fm_assert(clickable_hit_test(*cl, p));
    for (const auto p : {Vector2i{min.x() - 1, center.y()}, Vector2i{max.x(), center.y()},
                         Vector2i{center.x(), min.y() - 1}, Vector2i{center.x(), max.y()}})
        fm_assert(!clickable_hit_test(*cl, p));
}

void test_light_visibility()
{
    world w;
    constexpr chunk_coords_ ch{0, 0, 0};
    const auto L = w.make_object<light>(w.make_id(), global_coords{ch, local_coords{8, 8}}, light_proto{});
    const auto origin = tile_shader::point_to_pixelʹ(Vector3i(L->position()), win, {});

    const auto at = [&](Vector2i pixel) { return clickable_for_object(*L, win, pixel - origin); };
    fm_assert(at({0, win.y()/2}));
    fm_assert(at({win.x() - 1, win.y()/2}));
    fm_assert(at({win.x()/2, 0}));
    fm_assert(at({win.x()/2, win.y() - 1}));
    fm_assert(!at({win.x() + 100, win.y()/2}));
    fm_assert(!at({-100, win.y()/2}));
    fm_assert(!at({win.x()/2, win.y() + 100}));
    fm_assert(!at({win.x()/2, -100}));
    fm_assert(!at({win.x() + 100, win.y() + 100}));
    fm_assert(!at({-100, -100}));
    fm_assert(!at({win.x() + 100, -100}));
    fm_assert(!at({-100, win.y() + 100}));
}

void test_light_depth()
{
    world w;
    constexpr chunk_coords_ ch{0, 0, 0};
    const auto nw = w.make_object<light>(w.make_id(), global_coords{ch, local_coords{8, 8}}, light_proto{});
    const auto se = w.make_object<light>(w.make_id(), global_coords{ch, local_coords{9, 9}}, light_proto{});
    const auto camera = camera_centering(*nw);
    const auto cl_nw = clickable_for_object(*nw, win, camera), cl_se = clickable_for_object(*se, win, camera);
    fm_assert(cl_nw && cl_se);
    fm_assert(cl_se->depth > cl_nw->depth);
}

} // namespace

void Test::test_clickable()
{
    test_light_icon();
    test_light_visibility();
    test_light_depth();
}

} // namespace floormat
