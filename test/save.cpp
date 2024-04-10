#include "app.hpp"
#include "loader/anim-cell.hpp"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "src/scenery.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/ground-atlas.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-iterator.hpp"
#include "src/nanosecond.inl"
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
        const auto end = e.atlas->info().nframes-1;
        constexpr auto dt = Second / 60;
        fm_assert(e.frame == end);
        {   auto& x = std::get<door_scenery>(e.subtype);
            fm_assert(!x.active);
            e.activate(e.index());
            fm_assert(x.active);
            { auto index = e.index(); e.update(index, dt); }
            fm_assert(e.frame != end);
            for (int i = 0; i < 60*3; i++)
                { auto index = e.index(); e.update(index, dt); }
            fm_assert(e.frame == 0);
            fm_assert(!x.active);
        }
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
        const auto type = ae.type();
        fm_assert(ae.type() == be.type());
        fm_assert(type < object_type::COUNT && type != object_type::none);
        switch (type)
        {
        case object_type::none:
        case object_type::COUNT: std::unreachable();
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
        }
    }
}

void assert_chunks_equal(const chunk* a, const chunk* b)
{
    fm_assert(a);
    fm_assert(b);
    assert_chunks_equal(*a, *b);
}

[[nodiscard]] world reload_from_save(StringView tmp, world& w)
{
    if (Path::exists(tmp))
        Path::remove(tmp);
    w.serialize(tmp);
    return world::deserialize(tmp, loader_policy::error);
}

void run(StringView input, StringView tmp)
{
    if (Path::exists(tmp))
        Path::remove(tmp);
    chunk_coords_ coord{};
    world w;
    if (input)
        w = world::deserialize(input, loader_policy::ignore);
    else
    {
        coord = {1, 1, 0};
        w = world();
        auto& c = test_app::make_test_chunk(w, coord);
        fm_assert(!c.empty(true));
    }
    w.serialize(tmp);
    auto w2 = world::deserialize(tmp, loader_policy::ignore);
    auto& c2 = w2[coord];
    fm_assert(!c2.empty(true));
    assert_chunks_equal(w[coord], c2);
}

void test_save_1()
{
    const auto tmp_filename = Path::join(loader.TEMP_PATH, "test/test-save1.dat"_s);

    run({}, tmp_filename);
}

void test_save_2()
{
    const auto tmp_filename = Path::join(loader.TEMP_PATH, "test/test-save2.dat"_s);
    const auto dir = Path::join(loader.TEMP_PATH, "test/save/"_s);
    using LF = Path::ListFlag;
    auto files = Path::list(dir, LF::SkipDirectories|LF::SkipSpecial|LF::SkipDotAndDotDot);
    fm_assert(files);
    for (const StringView file : *files)
    {
        fm_assert(file.hasSuffix(".dat"_s));
        auto path = Path::join(dir, file);
        run(path, tmp_filename);
    }
}

void test_save_objs()
{
    const auto tmp = Path::join(loader.TEMP_PATH, "test/test-save-objs.dat"_s);

    // todo! test all object and scenery types!

    {   // --- counter ---
        auto w = world();

        const auto ctr = w.object_counter();
        const auto ctrʹ  = ctr + 364;
        fm_assert(ctrʹ > ctr);
        fm_assert(ctrʹ + 2 > ctr);
        w.set_object_counter(ctrʹ);
        (void)w.make_id(); (void)w.make_id();
        const auto ctrʹʹ = w.object_counter();
        fm_assert(ctrʹʹ == ctrʹ + 2);

        auto w2 = reload_from_save(tmp, w);
        const auto ctrʹʹʹ = w.object_counter();
        fm_assert(ctrʹʹʹ == ctrʹʹ);
    }

    {   // ---  critter ---
        auto w = world();
        critter_proto p;
        p.atlas        = loader.anim_atlas("npc-walk", loader.ANIM_PATH);
        p.offset       = Vector2b{-1, 2};
        p.bbox_offset  = Vector2b{3, -4};
        p.bbox_size    = Vector2ub{129, 254};
        p.delta        = uint32_t{65638};
        p.frame        = uint16_t{9};
        p.type         = object_type::critter;
        p.r            = rotation::SE;
        p.pass         = pass_mode::see_through;
        p.name         = "foo 123"_s;
        p.speed        = 0.25f;
        p.playable     = true;

        constexpr auto ch = chunk_coords_{512, -768, 0};
        constexpr auto coord = global_coords{ch, {1, 15}};
        constexpr auto offset_frac = uint16_t{44'432};

        const auto objʹ = w.make_object<critter>(w.make_id(), coord, p);
        fm_assert(objʹ);
        const auto& obj = *objʹ;
        const_cast<uint16_t&>(obj.offset_frac_) = offset_frac;
        auto w2 = reload_from_save(tmp, w);
        const auto& obj2ʹ = w.find_object<critter>(obj.id);
        fm_assert(obj2ʹ);
        const auto& obj2 = *obj2ʹ;
        fm_assert(p.name == obj2.name);
        fm_assert(p.frame == obj2.frame);
        fm_assert(p.speed == obj2.speed);
        fm_assert(obj.offset_frac_ == obj2.offset_frac_);

        assert_chunks_equal(w.at(ch), w2.at(ch));
    }

#if 0
    constexpr auto coord = global_coords{{ 1, -2, 0}, { 6, 5}};
    constexpr auto coord = global_coords{{-3,  4, 0}, { 3, 4}};
    constexpr auto coord = global_coords{{ 5, -6, 0}, { 4, 7}};
    constexpr auto coord = global_coords{{-7,  8, 0}, { 9, 1}};
    constexpr auto coord = global_coords{{ 9,  0, 0}, {15, 0}};
#endif

    {

    }
}

} // namespace

void test_app::test_save()
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    test_save_1();
}

void test_app::test_saves()
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    test_save_2();
    test_save_objs();
}

} // namespace floormat
