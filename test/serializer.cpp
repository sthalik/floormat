#include "app.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "src/scenery.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/ground-atlas.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-iterator.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat {

namespace Path = Corrade::Utility::Path;

chunk& test_app::make_test_chunk(world& w, chunk_coords_ ch)
{
    chunk& c = w[ch];
    c.mark_modified();

    auto metal2 = loader.wall_atlas("empty", loader_policy::warn);
    auto tiles  = loader.ground_atlas("tiles");
    auto door = loader.scenery("door1");
    auto table = loader.scenery("table1");
    auto control_panel = loader.scenery("control panel (wall) 1");

    constexpr auto N = TILE_MAX_DIM;
    for (auto [x, k, pt] : c)
        x.ground() = { tiles, variant_t(k % tiles->num_tiles()) };
    control_panel.r = rotation::W;
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north() = { metal2, 0 };
    c[{K,   K  }].wall_west()  = { metal2, 0 };
    c[{K,   K+1}].wall_north() = { metal2, 0 };
    c[{K+1, K  }].wall_west()  = { metal2, 0 };
    w.make_object<scenery>(w.make_id(), {ch, {3, 4}}, table);
    w.make_object<scenery>(w.make_id(), {ch, {K, K+1}}, control_panel);

    const auto add_player = [&](StringView name, Vector2i coord, bool playable) {
        critter_proto cproto;
        cproto.name = name;
        cproto.playable = playable;
        auto& p = *w.make_object<critter>(w.make_id(), global_coords{ch, {coord.x(), coord.y()}}, cproto);
        p.frame = (uint16_t)coord.x();
    };
    add_player("Player 1", {12, 11}, true); // duplicate
    add_player("Player 1", {13, 11}, true); // duplicate
    add_player("Player 2", {14, 11}, false);
    add_player("Player 3", {15, 11}, true);

    {
        auto& e = *w.make_object<scenery>(w.make_id(), {ch, {K+3, K+1}}, door);
        const auto index = e.index();
        const auto end = e.atlas->info().nframes-1;
        fm_assert(e.frame == end);
        fm_assert(!e.active);
        e.activate(e.index());
        fm_assert(e.active);
        e.update(index, 1.f/60);
        fm_assert(e.frame != end);
        for (int i = 0; i < 60*3; i++)
            e.update(index, 1.f/60);
        fm_assert(e.frame == 0);
        fm_assert(!e.active);
    }
    return c;
}

namespace {

void assert_chunks_equal(const chunk& a, const chunk& b)
{
    fm_assert(a.objects().size() == b.objects().size());

    for (auto i = 0uz; i < TILE_COUNT; i++)
    {
        const auto &a1 = a[i], &b1 = b[i];
        fm_assert(a1 == b1);
    }

    for (auto i = 0uz; i < a.objects().size(); i++)
    {
        const auto& ae = *a.objects()[i];
        const auto& be = *b.objects()[i];
        fm_assert(ae.type() == be.type());
        switch (ae.type())
        {
        case object_type::critter: {
            const auto& e1 = static_cast<const critter&>(ae);
            const auto& e2 = static_cast<const critter&>(be);
            const auto p1 = critter_proto(e1), p2 = critter_proto(e2);
            fm_assert(p1 == p2);
            break;
        }
        case object_type::scenery: {
            const auto& e1 = static_cast<const scenery&>(ae);
            const auto& e2 = static_cast<const scenery&>(be);
            const auto p1 = scenery_proto(e1), p2 = scenery_proto(e2);
            fm_assert(p1 == p2);
            break;
        }
        case object_type::light: {
            const auto& e1 = static_cast<const light&>(ae);
            const auto& e2 = static_cast<const light&>(be);
            const auto p1 = light_proto(e1), p2 = light_proto(e2);
            fm_assert(p1 == p2);
            break;
        }
        default:
            fm_abort("invalid object type '%d'", (int)ae.type());
        }
    }
}

void test_serializer(StringView input, StringView tmp)
{
    if (Path::exists(tmp))
        Path::remove(tmp);
    chunk_coords_ coord{};
    world w;
    if (input)
        w = world::deserialize(input);
    else
    {
        coord = {1, 1, 0};
        w = world();
        auto& c = test_app::make_test_chunk(w, coord);
        fm_assert(!c.empty(true));
    }
    w.serialize(tmp);
    auto w2 = world::deserialize(tmp);
    auto& c2 = w2[coord];
    fm_assert(!c2.empty(true));
    assert_chunks_equal(w[coord], c2);
}

} // namespace

void test_app::test_serializer_1()
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    const auto tmp_filename = Path::join(loader.TEMP_PATH, "test/test-serializer1.dat"_s);
    test_serializer({}, tmp_filename);
}

void test_app::test_serializer_2()
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    const auto tmp_filename = Path::join(loader.TEMP_PATH, "test/test-serializer2.dat"_s);
    const auto dir = Path::join(loader.TEMP_PATH, "test/save/"_s);
    using LF = Path::ListFlag;
    auto files = Path::list(dir, LF::SkipDirectories|LF::SkipSpecial|LF::SkipDotAndDotDot);
    fm_assert(files);
    for (const StringView file : *files)
    {
        fm_assert(file.hasSuffix(".dat"_s));
        auto path = Path::join(dir, file);
        test_serializer(path, tmp_filename);
    }
}

} // namespace floormat
