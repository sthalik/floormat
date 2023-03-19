#include "app.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "src/scenery.hpp"
#include "src/character.hpp"
#include "src/tile-atlas.hpp"
#include "src/anim-atlas.hpp"
#include <Corrade/Utility/Path.h>

namespace floormat {

namespace Path = Corrade::Utility::Path;

namespace {

chunk& make_test_chunk(world& w, chunk_coords ch)
{
    chunk& c = w[ch];
    c.mark_modified();
    auto metal1 = loader.tile_atlas("metal1", {2, 2}, pass_mode::pass),
         metal2 = loader.tile_atlas("metal2", {2, 2}, pass_mode::blocked),
         tiles  = loader.tile_atlas("tiles", {8, 5}, pass_mode::pass);
    constexpr auto N = TILE_MAX_DIM;
    for (auto [x, k, pt] : c)
        x.ground() = { tiles, variant_t(k % tiles->num_tiles()) };
    auto door = loader.scenery("door1"),
         table = loader.scenery("table1"),
         control_panel = loader.scenery("control panel (wall) 1");
    control_panel.r = rotation::W;
    constexpr auto K = N/2;
    c[{K,   K  }].wall_north() = { metal1, 0 };
    c[{K,   K  }].wall_west()  = { metal2, 0 };
    c[{K,   K+1}].wall_north() = { metal1, 0 };
    c[{K+1, K  }].wall_west()  = { metal2, 0 };
    w.make_entity<scenery>(w.make_id(), {ch, {3, 4}}, table);
    w.make_entity<scenery>(w.make_id(), {ch, {K, K+1}}, control_panel);
    {
        auto& e = *w.make_entity<scenery>(w.make_id(), {ch, {K+3, K+1}}, door);
        auto i = e.index();
        e.activate(i);
        e.update(i, 1.f/60);
        fm_assert(e.active);
        fm_assert(e.frame != 0 && e.frame != e.atlas->info().nframes - 1);
    }
    return c;
}

void assert_chunks_equal(const chunk& a, const chunk& b)
{
    fm_assert(a.entities().size() == b.entities().size());

    for (auto i = 0_uz; i < TILE_COUNT; i++)
    {
        const auto &a1 = a[i], &b1 = b[i];
        fm_assert(a1 == b1);
    }

    for (auto i = 0_uz; i < a.entities().size(); i++)
    {
        const auto& ae = *a.entities()[i];
        const auto& be = *b.entities()[i];
        fm_assert(ae.type == be.type);
        switch (ae.type)
        {
        case entity_type::character: {
            const auto& e1 = static_cast<const character&>(ae);
            const auto& e2 = static_cast<const character&>(be);
            const auto p1 = character_proto(e1), p2 = character_proto(e2);
            fm_assert(p1 == p2);
            break;
        }
        case entity_type::scenery: {
            const auto& e1 = static_cast<const scenery&>(ae);
            const auto& e2 = static_cast<const scenery&>(be);
            const auto p1 = scenery_proto(e1), p2 = scenery_proto(e2);
            fm_assert(p1 == p2);
            break;
        }
        default:
            fm_abort("invalid entity type '%d'", (int)ae.type);
        }
    }
}

void test_serializer(StringView input, StringView tmp)
{
    if (Path::exists(tmp))
        Path::remove(tmp);
    chunk_coords coord{};
    world w;
    if (input)
        w = world::deserialize(input);
    else
    {
        coord = {1, 1};
        w = world();
        make_test_chunk(w, coord);
    }
    w.serialize(tmp);
    auto w2 = world::deserialize(tmp);
    auto& c2 = w2[coord];
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
