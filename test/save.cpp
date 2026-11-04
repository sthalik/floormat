#include "app.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/object-storage.inl"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/hole.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "src/anim-atlas.hpp"
#include "src/nanosecond.inl"
#include "loader/loader.hpp"
#include "loader/anim-cell.hpp"
#include "loader/ground-cell.hpp"
#include "loader/wall-cell.hpp"
#include "loader/scenery-cell.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/crc64.hpp"
#include "compat/exception.hpp"
#include "serialize/binary-serializer.hpp"
#include <bit>
#include <cstdio>
#include <cstring>
#include <cr/Path.h>
#include <mg/Color.h>
#include <mg/Functions.h>

namespace floormat {

namespace Path = Corrade::Utility::Path;

chunk& Test::make_test_chunk(world& w, chunk_coords_ ch)
{
    chunk& c = w[ch];
    c.mark_modified();

    auto metal2 = loader.wall_atlas("empty", loader_policy::warn);
    auto tiles  = loader.ground_atlas("tiles");
    auto door = loader.scenery("door1");
    auto table = loader.scenery("table1");
    auto control_panel = loader.scenery("control panel (wall) 1");

    for (auto k = 0u; k < TILE_COUNT; k++)
        c[k].ground() = { tiles, variant_t(k % tiles->num_tiles()) };
    control_panel.r = rotation::W;
    constexpr auto K = TILE_MAX_DIM/2;
    c[{K,   K  }].wall_north() = { metal2, 0 };
    c[{K,   K  }].wall_west()  = { metal2, 0 };
    c[{K,   K+1}].wall_north() = { metal2, 0 };
    c[{K+1, K  }].wall_west()  = { metal2, 0 };
    w.make_scenery(w.make_id(), {ch, {3, 4}}, scenery_proto(table));
    w.make_scenery(w.make_id(), {ch, {K, K+1}}, scenery_proto(control_panel));
    auto L = light_proto{};
    L.color = {64, 128, 252, 201};
    L.max_distance = float{0.125};
    L.falloff = light_falloff::quadratic;
    L.enabled = false;
    w.make_object<light>(w.make_id(), {ch, {4, 1}}, L);

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
        auto eʹ = w.make_scenery(w.make_id(), {ch, {K+3, K+1}}, scenery_proto(door));
        fm_assert(eʹ->scenery_type() == scenery_type::door);
        auto& e = static_cast<door_scenery&>(*eʹ);
        const auto end = e.atlas->info().nframes-1;
        constexpr auto dt = Second / 60;
        fm_assert(e.frame == end);
        {   fm_assert(!e.active);
            e.activate(e.index());
            fm_assert(e.active);
            { auto index = e.index(); e.update(eʹ, index, dt); }
            fm_assert(e.frame != end);
            for (int i = 0; i < 60*3; i++)
                { auto index = e.index(); e.update(eʹ, index, dt); }
            fm_assert(e.frame == 0);
            fm_assert(!e.active);
        }
    }
    return c;
}

namespace {

// Each load makes placeholders of its own, so the same missing atlas comes back as a new pointer.
template<typename Atlas>
void match_placeholders(const bptr<Atlas>& a, bptr<Atlas>& b)
{
    if (a && b && a->is_placeholder() && b->is_placeholder() && a->name() == b->name())
        b = a;
}

void assert_chunks_equal(const chunk& a, const chunk& b)
{
    fm_assert(a.objects().size() == b.objects().size());

    for (auto i = 0uz; i < TILE_COUNT; i++)
    {
        const auto a1 = tile_proto(a[i]);
        auto b1 = tile_proto(b[i]);
        match_placeholders(a1.ground_atlas, b1.ground_atlas);
        match_placeholders(a1.wall_north_atlas, b1.wall_north_atlas);
        match_placeholders(a1.wall_west_atlas, b1.wall_west_atlas);
        fm_assert(a1 == b1);
    }

    for (auto i = 0u; i < a.objects().size(); i++)
    {
        const auto& ae = a.objects()[i];
        const auto& be = b.objects()[i];
        const auto type = ae.type();
        fm_assert(ae.type() == be.type());
        fm_assert(type < object_type::COUNT && type != object_type::none);
        switch (type)
        {
        case object_type::none:
        case object_type::COUNT:
            fm_assert(false);
        case object_type::critter: {
            const auto& e1 = static_cast<const critter&>(ae);
            const auto& e2 = static_cast<const critter&>(be);
            const auto p1 = critter_proto(e1);
            auto p2 = critter_proto(e2);
            match_placeholders(p1.atlas, p2.atlas);
            fm_assert(p1 == p2);
            break;
        }
        case object_type::scenery: {
            const auto& e1 = static_cast<const scenery&>(ae);
            const auto& e2 = static_cast<const scenery&>(be);
            const auto p1 = scenery_proto(e1);
            auto p2 = scenery_proto(e2);
            match_placeholders(p1.atlas, p2.atlas);
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
        case object_type::hole: {
            const auto& e1 = static_cast<const hole&>(ae);
            const auto& e2 = static_cast<const hole&>(be);
            const auto p1 = hole_proto(e1), p2 = hole_proto(e2);
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

// The writer runs the reader's checks, so bad data has to be put into the file directly.
void write_patched(StringView path, ArrayView<const char> save, ArrayView<const char> from, ArrayView<const char> to)
{
    constexpr auto crc_size = (uint32_t)sizeof(uint64_t);
    fm_assert(save.size() > crc_size);
    fm_assert(from.size() == to.size());
    const auto body = (uint32_t)save.size() - crc_size;
    auto pos = (uint32_t)-1, hits = 0u;
    for (auto i = 0u; i + from.size() <= body; i++)
        if (std::memcmp(&save[i], from.data(), from.size()) == 0)
        {
            pos = i;
            hits++;
        }
    fm_assert(hits == 1);

    auto buf = Array<char>{NoInit, save.size()};
    std::memcpy(buf.data(), save.data(), save.size());
    std::memcpy(&buf[pos], to.data(), to.size());
    const auto crc = Serialize::maybe_byteswap(Hash::crc64_update(Hash::CRC64_INITIALIZER, buf.data(), body));
    std::memcpy(&buf[body], &crc, sizeof crc);
    fm_assert(Path::write(path, buf));
}

void write_patched(StringView path, ArrayView<const char> save, float from, uint32_t to)
{
    write_patched(path, save, {(const char*)&from, sizeof from}, {(const char*)&to, sizeof to});
}

template<typename T>
void check_nan_rejected(StringView path, ArrayView<const char> save, object_id id, float T::* field, float value)
{
    // A valid edit must load, so the NaN load can only fail on the value.
    const auto other = value * 2;
    write_patched(path, save, value, std::bit_cast<uint32_t>(other));
    auto w = world::deserialize(path, loader_policy::error);
    const auto obj = w.find_object<T>(id);
    fm_assert(obj);
    fm_assert((*obj).*field == other);

    // Release builds assume no NaN, and clang compiles quiet_NaN() here to 0.
    constexpr uint32_t nan_bits = 0x7fc00000;
    write_patched(path, save, value, nan_bits);
    bool caught = false;
    try { (void)world::deserialize(path, loader_policy::error); }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(caught);
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
        auto& c = Test::make_test_chunk(w, coord);
        fm_assert(!c.empty(true));
    }
    w.serialize(tmp);
    auto w2 = world::deserialize(tmp, loader_policy::ignore);
    auto& c2 = w2[coord];
    fm_assert(!c2.empty(true));
    assert_chunks_equal(w[coord], c2);
    w.chunk_table_prepare_frame();
    w2.chunk_table_prepare_frame();
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
        const auto ctrʹʹʹ = w2.object_counter();
        fm_assert(ctrʹʹʹ == ctrʹʹ);
    }

    {   // --- counter at the object table's limit ---
        constexpr object_id max_id = (object_id{1} << object_table::key_bits) - 1;
        auto w = world();
        w.set_object_counter(max_id);
        auto w2 = reload_from_save(tmp, w);
        fm_assert(w2.object_counter() == max_id);

        w.set_object_counter(max_id + 1);
        bool caught = false;
        try { (void)reload_from_save(tmp, w); }
        catch (const floormat::exception&) { caught = true; }
        fm_assert(caught);
    }

    {   // ---  critter ---
        auto w = world();
        critter_proto p;
        p.atlas        = loader.anim_atlas("npc-walk", loader.ANIM_PATH);
        p.offset       = Vector2b{-1, 2};
        p.bbox_offset  = Vector2b{3, -4};
        p.bbox_size    = Vector2ub{129, 255};
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
        constexpr auto anim_progress = uint32_t{44'432};

        const auto objʹ = w.make_object<critter>(w.make_id(), coord, p);
        fm_assert(objʹ);
        const auto& obj = *objʹ;
        const_cast<uint32_t&>(obj.anim_progress) = anim_progress;
        auto w2 = reload_from_save(tmp, w);
        const auto& obj2ʹ = w2.find_object<critter>(obj.id);
        fm_assert(obj2ʹ);
        const auto& obj2 = *obj2ʹ;
        fm_assert(p.name == obj2.name);
        fm_assert(p.frame == obj2.frame);
        fm_assert(p.speed == obj2.speed);
        fm_assert(obj2.anim_progress == anim_progress);

        assert_chunks_equal(w.at(ch), w2.at(ch));
    }
    {
        // ---  light ---
        auto w = world();
        light_proto p;
        p.max_distance = 42;
        p.color = Color4ub{1, 2, 3, 4};
        p.falloff = light_falloff::quadratic;
        p.enabled = false;
        p.frame = 0;
        p.offset = {1, 2};
        p.bbox_size = {3, 2};
        p.delta = 5;
        p.pass = pass_mode::see_through;

        constexpr auto ch = chunk_coords_{ 1, -2, 0};
        constexpr auto coord = global_coords{ch, {6, 5}};

        const auto id = w.make_id();
        const auto objʹ = w.make_object<light>(id, coord, p);
        auto w2 = reload_from_save(tmp, w);
        const auto& obj2ʹ = w2.find_object<light>(id);
        fm_assert(obj2ʹ);
        const auto& obj2 = *obj2ʹ;
        fm_assert(p.max_distance == obj2.max_distance);
        fm_assert(p.color == obj2.color);
        fm_assert(p.falloff == obj2.falloff);
        fm_assert(p.enabled == obj2.enabled);
        assert_chunks_equal(w.at(ch), w2.at(ch));
        //const auto obj3 = w.find_object<generic_scenery>(id); // must fail
    }

    {
        // ---  scenery ---
        auto w = world();
        scenery_proto p;
        p.atlas = loader.invalid_scenery_atlas().proto->atlas;
        p.subtype = generic_scenery_proto{};
        constexpr auto ch = chunk_coords_{-3,  4, 0};
        constexpr auto coord = global_coords{ch, { 3, 4}};
        const auto id = w.make_id();
        const auto objʹ = w.make_scenery(id, coord, move(p));
        const auto obj = static_pointer_cast<generic_scenery>(objʹ);
        const auto obj2 = w.find_object<generic_scenery>(id);
        //const auto obj3 = w.find_object<door_scenery>(id); // must fail
        fm_assert(obj == obj2);
    }

#if 0
    constexpr auto coord = global_coords{{ 5, -6, 0}, { 4, 7}};
    constexpr auto coord = global_coords{{-7,  8, 0}, { 9, 1}};
    constexpr auto coord = global_coords{{ 9,  0, 0}, {15, 0}};
#endif
}

void test_save_nan()
{
    const auto tmp = Path::join(loader.TEMP_PATH, "test/test-save-nan.dat"_s);
    const auto patched = Path::join(loader.TEMP_PATH, "test/test-save-nan-patched.dat"_s);
    constexpr auto ch = chunk_coords_{-5, 6, 0};

    auto w = world();
    critter_proto cp;
    cp.name = "test"_s;
    cp.speed = 1.7182817f;
    cp.anim_speed = 3.1415927f;
    const auto cid = w.make_id();
    w.make_object<critter>(cid, {ch, {2, 3}}, cp);
    light_proto lp;
    lp.max_distance = 2.7182817f;
    lp.radius = 7.389056f;
    const auto lid = w.make_id();
    w.make_object<light>(lid, {ch, {4, 5}}, lp);
    w.serialize(tmp);
    const auto save = Path::read(tmp);
    fm_assert(save);

    check_nan_rejected(patched, *save, cid, &critter::speed, cp.speed);
    check_nan_rejected(patched, *save, cid, &critter::anim_speed, cp.anim_speed);
    check_nan_rejected(patched, *save, lid, &light::max_distance, lp.max_distance);
    check_nan_rejected(patched, *save, lid, &light::radius, lp.radius);
}

// Each critter is named after the offset_frac and delta it was saved with. Returns how many ns its
// delta is from that pair converted at its atlas's fps.
double offset_frac_error(const critter& C)
{
    uint32_t offset_frac, delta;
    fm_assert(std::sscanf(C.name.data(), "offset_frac=%u delta=%u", &offset_frac, &delta) == 2);
    // delta counted (uint32_t)-1 per second. offset_frac is path length × 32768, and a
    // pixel of path cost a frame.
    const auto expected = delta * 1e9 / double(uint32_t(-1))
                        + offset_frac * 1e9 / (32768 * double(C.atlas->info().fps));
    return Math::abs(double(C.delta) - expected);
}

void check_offset_frac(StringView name, uint16_t expected_proto, uint32_t expected_count)
{
    constexpr auto magic = ".floormat.save"_s;
    const auto path = Path::join(loader.TEMP_PATH, Path::join("test/save"_s, name));
    const auto file = Path::read(path);
    fm_assert(file && file->size() > magic.size() + sizeof(uint16_t));
    uint16_t proto;
    std::memcpy(&proto, file->data() + magic.size(), sizeof proto);
    fm_assert(Serialize::maybe_byteswap(proto) == expected_proto);

    auto w = world::deserialize(path, loader_policy::ignore);
    uint32_t count = 0;
    double max_err = 0;
    for (auto& c : w.chunks())
        c.objects().visit<critter>([&](critter& C) {
            max_err = Math::max(max_err, offset_frac_error(C));
            count++;
        });
    fm_assert(count == expected_count);
    fm_assert(max_err < 5);
}

// The proto 29 save is quicksave.dat plus 24 critters, the proto 30 one 24 critters facing N.
void test_save_offset_frac()
{
    check_offset_frac("proto-29-offset-frac.dat"_s, 29, 25);
    check_offset_frac("proto-30-offset-frac.dat"_s, 30, 24);
}

void check_missing_atlas(StringView name, uint32_t expected_count)
{
    const auto path = Path::join(loader.TEMP_PATH, Path::join("test/save"_s, name));
    const auto patched = Path::join(loader.TEMP_PATH, "test/test-save-missing-atlas.dat"_s);
    const auto file = Path::read(path);
    fm_assert(file);
    constexpr auto from = "anim/npc-walk"_s, to = "anim/npc-gone"_s;
    write_patched(patched, *file, {from.data(), from.size()}, {to.data(), to.size()});

    auto w = world::deserialize(patched, loader_policy::ignore);
    uint32_t count = 0;
    double max_err = 0;
    for (auto& c : w.chunks())
        c.objects().visit<critter>([&](critter& C) {
            fm_assert(C.atlas->name() == to);
            fm_assert(C.atlas->is_placeholder());
            fm_assert(C.atlas->info().fps == 30);
            fm_assert(C.frame == 0);
            // quicksave.dat's player was saved facing SE on frame 23.
            if (C.playable)
                fm_assert(C.r == rotation::SE);
            max_err = Math::max(max_err, offset_frac_error(C));
            count++;
        });
    fm_assert(count == expected_count);
    fm_assert(max_err < 5);

    const auto resaved = Path::join(loader.TEMP_PATH, "test/test-save-missing-atlas-2.dat"_s);
    if (Path::exists(resaved))
        Path::remove(resaved);
    w.serialize(resaved);
    auto w2 = world::deserialize(resaved, loader_policy::ignore);
    count = 0;
    for (auto& c : w2.chunks())
        c.objects().visit<critter>([&](critter& C) {
            fm_assert(C.atlas->name() == to);
            fm_assert(C.atlas->is_placeholder());
            fm_assert(C.atlas->info().fps == 30);
            count++;
        });
    fm_assert(count == expected_count);
    fm_assert(loader.anim_atlas(to, {}, loader_policy::ignore) == loader.invalid_anim_atlas().atlas);
}

void test_save_missing_atlas()
{
    check_missing_atlas("proto-29-offset-frac.dat"_s, 25);
    check_missing_atlas("proto-30-offset-frac.dat"_s, 24);
}

void test_save_missing_tile_atlas()
{
    const auto tmp = Path::join(loader.TEMP_PATH, "test/test-save-missing-tile-atlas.dat"_s);
    const auto patched = Path::join(loader.TEMP_PATH, "test/test-save-missing-tile-atlas-2.dat"_s);
    const auto resaved = Path::join(loader.TEMP_PATH, "test/test-save-missing-tile-atlas-3.dat"_s);
    constexpr auto ground_from = "tiles"_s, ground_to = "gone1"_s;
    constexpr auto wall_from = "test1"_s, wall_to = "gone2"_s;
    constexpr auto ch = chunk_coords_{2, -3, 0};
    constexpr auto pos = local_coords{4, 5};

    const auto& ground = loader.ground_atlas(ground_from);
    const auto variant = variant_t(ground->num_tiles() - 1);
    fm_assert(variant > 0);
    {
        auto w = world();
        auto& c = w[ch];
        c.mark_modified();
        c[pos].ground() = { ground, variant };
        c[pos].wall_north() = { loader.wall_atlas(wall_from), 0 };
        c[pos].wall_west() = { loader.wall_atlas(wall_from), 0 };
        if (Path::exists(tmp))
            Path::remove(tmp);
        w.serialize(tmp);
    }
    auto save = Path::read(tmp);
    fm_assert(save);
    write_patched(patched, *save, {ground_from.data(), ground_from.size()}, {ground_to.data(), ground_to.size()});
    save = Path::read(patched);
    fm_assert(save);
    write_patched(patched, *save, {wall_from.data(), wall_from.size()}, {wall_to.data(), wall_to.size()});

    const auto check = [&](world& w) {
        auto t = w[ch][pos];
        const auto G = t.ground_atlas();
        fm_assert(G->name() == ground_to);
        fm_assert(G->is_placeholder());
        fm_assert(t.ground().variant == variant);
        const auto N = t.wall_north_atlas(), W = t.wall_west_atlas();
        fm_assert(N->name() == wall_to);
        fm_assert(N->is_placeholder());
        fm_assert(N == W);
    };

    auto w = world::deserialize(patched, loader_policy::ignore);
    check(w);
    if (Path::exists(resaved))
        Path::remove(resaved);
    w.serialize(resaved);
    auto w2 = world::deserialize(resaved, loader_policy::ignore);
    check(w2);
    fm_assert(loader.ground_atlas(ground_to, loader_policy::ignore) == loader.invalid_ground_atlas().atlas);
    fm_assert(loader.wall_atlas(wall_to, loader_policy::ignore) == loader.invalid_wall_atlas().atlas);
}

// `name` with its last four characters replaced, so that write_patched() can swap one for the other.
String missing_name(StringView name)
{
    constexpr auto suffix = "gone"_s;
    fm_assert(name.size() > suffix.size());
    auto ret = String{name};
    for (uint32_t i = 0; i < suffix.size(); i++)
        ret[ret.size() - suffix.size() + i] = suffix[i];
    fm_assert(ret != name);
    return ret;
}

void test_save_missing_door_atlas()
{
    const auto tmp = Path::join(loader.TEMP_PATH, "test/test-save-missing-door-atlas.dat"_s);
    const auto patched = Path::join(loader.TEMP_PATH, "test/test-save-missing-door-atlas-2.dat"_s);
    const auto resaved = Path::join(loader.TEMP_PATH, "test/test-save-missing-door-atlas-3.dat"_s);
    constexpr auto ch = chunk_coords_{2, -3, 0};
    constexpr auto closed_pos = local_coords{4, 5}, open_pos = local_coords{6, 5}, table_pos = local_coords{8, 5};

    const auto& door_proto = loader.scenery("door1");
    const auto door_from = String{door_proto.atlas->name()};
    const auto table_from = String{loader.scenery("table1").atlas->name()};
    const auto door_to = missing_name(door_from), table_to = missing_name(table_from);
    object_id closed_id = 0, open_id = 0, table_id = 0;
    {
        auto w = world();
        w[ch].mark_modified();
        auto closed = door_proto, open = door_proto;
        closed.frame = uint16_t(door_proto.atlas->info().nframes - 1);
        closed.pass = pass_mode::blocked;
        open.frame = 0;
        open.pass = pass_mode::pass;
        closed_id = w.make_id();
        open_id = w.make_id();
        table_id = w.make_id();
        w.make_scenery(closed_id, {ch, closed_pos}, move(closed));
        w.make_scenery(open_id, {ch, open_pos}, move(open));
        w.make_scenery(table_id, {ch, table_pos}, scenery_proto(loader.scenery("table1")));
        if (Path::exists(tmp))
            Path::remove(tmp);
        w.serialize(tmp);
    }
    auto save = Path::read(tmp);
    fm_assert(save);
    write_patched(patched, *save, {door_from.data(), door_from.size()}, {door_to.data(), door_to.size()});
    save = Path::read(patched);
    fm_assert(save);
    write_patched(patched, *save, {table_from.data(), table_from.size()}, {table_to.data(), table_to.size()});

    const auto check = [&](world& w) {
        const auto check_door = [&](object_id id, uint16_t frame, pass_mode pass) {
            const auto d = w.find_object<door_scenery>(id);
            fm_assert(d);
            fm_assert(d->atlas->name() == door_to);
            fm_assert(d->atlas->is_placeholder());
            fm_assert(d->atlas->info().fps == 30);
            fm_assert(d->atlas->info().nframes == 2);
            fm_assert(d->frame == frame);
            fm_assert(d->pass == pass);
            fm_assert(d->is_dynamic());
        };
        check_door(closed_id, 1, pass_mode::blocked);
        check_door(open_id, 0, pass_mode::pass);
        const auto t = w.find_object<generic_scenery>(table_id);
        fm_assert(t);
        fm_assert(t->atlas->name() == table_to);
        fm_assert(t->atlas->is_placeholder());
        fm_assert(t->atlas->info().fps == 0);
        fm_assert(t->atlas->info().nframes == 1);
        fm_assert(t->frame == 0);
        fm_assert(!t->is_dynamic());
    };

    auto w = world::deserialize(patched, loader_policy::ignore);
    check(w);
    if (Path::exists(resaved))
        Path::remove(resaved);
    w.serialize(resaved);
    auto w2 = world::deserialize(resaved, loader_policy::ignore);
    check(w2);
    fm_assert(loader.anim_atlas(door_to, {}, loader_policy::ignore) == loader.invalid_anim_atlas().atlas);

    {
        const auto restored = Path::join(loader.TEMP_PATH, "test/test-save-missing-door-atlas-4.dat"_s);
        const auto last = uint16_t(door_proto.atlas->info().nframes - 1);
        fm_assert(last > 1);
        auto bytes = Path::read(resaved);
        fm_assert(bytes);
        write_patched(restored, *bytes, {door_to.data(), door_to.size()}, {door_from.data(), door_from.size()});
        bytes = Path::read(restored);
        fm_assert(bytes);
        write_patched(restored, *bytes, {table_to.data(), table_to.size()}, {table_from.data(), table_from.size()});
        auto w3 = world::deserialize(restored, loader_policy::error);

        const auto swing = [&](object_id id, uint16_t from, uint16_t to, pass_mode from_pass, pass_mode to_pass) {
            const auto obj = w3.find_object(id);
            const auto d = w3.find_object<door_scenery>(id);
            fm_assert(d);
            fm_assert(!d->atlas->is_placeholder());
            fm_assert(d->atlas->name() == door_from);
            fm_assert(d->frame == from && d->pass == from_pass && !d->active);
            auto index = d->index();
            fm_assert(d->activate(index));
            for (uint32_t i = 0; d->active; i++)
            {
                fm_assert(i < 10);
                d->update(obj, index, Second);
            }
            fm_assert(d->frame == to && d->pass == to_pass);
        };
        swing(closed_id, last, 0, pass_mode::blocked, pass_mode::pass);
        swing(open_id, 0, last, pass_mode::pass, pass_mode::blocked);
    }

    const auto toggle =[&](object_id id, uint16_t frame, pass_mode pass) {
        const auto obj = w.find_object(id);
        const auto d = w.find_object<door_scenery>(id);
        auto index = d->index();
        fm_assert(d->activate(index));
        d->update(obj, index, Second);
        fm_assert(d->frame == frame && d->pass == pass && !d->active);
    };
    toggle(closed_id, 0, pass_mode::pass);
    toggle(open_id, 1, pass_mode::blocked);
}

} // namespace

void Test::test_save()
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    test_save_1();
}

void Test::test_saves()
{
    fm_assert(Path::exists(Path::join(loader.TEMP_PATH, "CMakeCache.txt")));
    test_save_2();
    test_save_objs();
    test_save_nan();
    test_save_offset_frac();
    test_save_missing_atlas();
    test_save_missing_tile_atlas();
    test_save_missing_door_atlas();
}

} // namespace floormat
