#include "app.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/tile.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "src/critter.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/light.hpp"
#include "src/anim-atlas.hpp"
#include "loader/loader.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/nanosecond.inl"
#include "serialize/binary-serializer.hpp"
#include <cr/GrowableArray.h>
#include <cr/StringView.h>
#include <cstring>

namespace floormat::Test {

namespace {

constexpr u16 proto = 19;
constexpr auto chunk_magic = (u16)~0xc0d3, scenery_magic = (u16)~0xb00b;
constexpr u8 meta_ground = 1 << 2, meta_wall_n = 1 << 3, meta_wall_w = 1 << 4, meta_rle = 1 << 7;
constexpr u8 exact_bit = 1 << 7;
constexpr u32 rotation_shift = 8 - 1 - rotation_BITS;

template<typename T>
void put(Array<char>& out, T x)
{
    char bytes[sizeof x];
    const auto value = Serialize::maybe_byteswap(x);
    std::memcpy(bytes, &value, sizeof x);
    arrayAppend(out, ArrayView<const char>{bytes});
}

void put_string(Array<char>& out, StringView str)
{
    fm_assert(!str.find('\0'));
    arrayAppend(out, arrayView(str.data(), str.size()));
    arrayAppend(out, '\0');
}

void put_frame(Array<char>& out, u16 frame)
{
    if (frame <= 0xff)
        put(out, (u8)frame);
    else
        put(out, frame);
}

void put_variant(Array<char>& out, variant_t v)
{
    fm_assert(v <= 0xff);
    put(out, (u8)v);
}

void put_bbox(Array<char>& out, const object& e)
{
    put(out, e.bbox_offset[0]);
    put(out, e.bbox_offset[1]);
    put(out, e.bbox_size[0]);
    put(out, e.bbox_size[1]);
}

u8 object_flags(const object& e, u8 subtype_bits)
{
    const auto pass = std::to_underlying(e.pass);
    fm_assert(pass < 1 << pass_mode_BITS);
    return u8(pass | subtype_bits | (e.frame <= 0xff) << 7);
}

u16 encode_delta(u32 delta)
{
    const auto decode = [](u32 x) { return object::ns_to_delta(object::fraction_to_ns(x * 65536u)); };
    const auto approx = u32(double(delta) * 65536 / 1e9 + .5);
    for (auto x = approx > 0 ? approx - 1 : 0; x <= approx + 1 && x <= 0xffff; x++)
        if (decode(x) == delta)
            return (u16)x;
    fm_abort("delta %u can't be stored in proto %u", delta, (unsigned)proto);
}

struct writer
{
    explicit writer(ArrayView<const StringView> scenery_names);
    Array<char> write(world& w);

private:
    struct atlas_entry { const void* ptr; StringView name; Vector2ub num_tiles; };
    struct scenery_entry { StringView name; u8 flags; u16 frame; };

    u16 intern_atlas(const void* ptr, StringView name, Vector2ub num_tiles);
    u16 intern_scenery(StringView name, u8 flags, u16 frame);
    u32 intern_string(StringView str);
    StringView scenery_name(const scenery_proto& sc) const;

    void write_chunk(Array<char>& out, const chunk& c);
    void write_critter(Array<char>& out, const critter& C);
    void write_scenery(Array<char>& out, const scenery& sc);
    void write_light(Array<char>& out, const light& L);

    ArrayView<const StringView> scenery_names;
    Array<atlas_entry> atlases;
    Array<scenery_entry> sceneries;
    Array<StringView> strings;
};

writer::writer(ArrayView<const StringView> scenery_names) : scenery_names{scenery_names}
{
    arrayReserve(atlases, 16);
    arrayReserve(sceneries, 16);
    arrayReserve(strings, 16);
}

u16 writer::intern_atlas(const void* ptr, StringView name, Vector2ub num_tiles)
{
    for (u32 i = 0; i < atlases.size(); i++)
        if (atlases[i].ptr == ptr)
            return (u16)i;
    arrayAppend(atlases, atlas_entry{ptr, name, num_tiles});
    return u16(atlases.size() - 1);
}

u16 writer::intern_scenery(StringView name, u8 flags, u16 frame)
{
    for (u32 i = 0; i < sceneries.size(); i++)
        if (sceneries[i].name == name && sceneries[i].flags == flags && sceneries[i].frame == frame)
            return (u16)i;
    arrayAppend(sceneries, scenery_entry{name, flags, frame});
    return u16(sceneries.size() - 1);
}

u32 writer::intern_string(StringView str)
{
    for (u32 i = 0; i < strings.size(); i++)
        if (strings[i] == str)
            return i;
    arrayAppend(strings, str);
    return u32(strings.size() - 1);
}

StringView writer::scenery_name(const scenery_proto& sc) const
{
    for (const auto& name : scenery_names)
        if (loader.scenery(name).atlas == sc.atlas)
            return name;
    fm_abort("no scenery name for atlas '%s'", sc.atlas->name().data());
}

void writer::write_critter(Array<char>& out, const critter& C)
{
    const critter_proto def;
    const bool exact = C.bbox_offset == def.bbox_offset && C.bbox_size == def.bbox_size;
    put(out, u8((exact ? exact_bit : 0) | u8(C.r) << rotation_shift));
    put(out, object_flags(C, u8(C.playable << 2)));
    put_frame(out, C.frame);
    put(out, u16{0});
    put(out, u16{0});
    put(out, intern_string(C.name));
    if (!exact)
        put_bbox(out, C);
}

void writer::write_scenery(Array<char>& out, const scenery& sc)
{
    const auto proto = scenery_proto(sc);
    const auto name = scenery_name(proto);
    const auto& base = loader.scenery(name);
    fm_assert(base.scenery_type() == proto.scenery_type());

    u8 bits = 0;
    bool active = false;
    if (const auto* g = swl::get_if<generic_scenery_proto>(&proto.subtype))
    {
        active = g->active;
        bits = u8(g->active << 2 | g->interactive << 4);
    }
    else if (const auto* d = swl::get_if<door_scenery_proto>(&proto.subtype))
    {
        active = d->active;
        bits = u8(d->active << 2 | d->closing << 3 | d->interactive << 4);
    }
    else
        fm_abort("scenery %llu has no subtype", (unsigned long long)sc.id);
    fm_assert(active || sc.delta == 0);

    const auto flags = object_flags(sc, bits);
    const bool exact = sc.bbox_offset == base.bbox_offset && sc.bbox_size == base.bbox_size && sc.delta == 0;
    put(out, intern_scenery(name, flags, sc.frame));
    put(out, u8((exact ? exact_bit : 0) | u8(sc.r) << rotation_shift));
    if (!exact)
    {
        put(out, flags);
        put_frame(out, sc.frame);
        put_bbox(out, sc);
        if (active)
            put(out, encode_delta(sc.delta));
    }
}

void writer::write_light(Array<char>& out, const light& L)
{
    const light_proto def;
    const bool exact = L.frame == 0 && L.pass == def.pass &&
                       L.bbox_offset == def.bbox_offset && L.bbox_size == def.bbox_size;
    const auto falloff = std::to_underlying(L.falloff);
    fm_assert(falloff < 1 << light_falloff_BITS);
    put(out, u8(exact | u8(L.r) << 1 | falloff << 4 | L.enabled << 7));
    put(out, L.max_distance);
    for (auto i = 0u; i < 4; i++)
        put(out, L.color[i]);
    if (!exact)
    {
        fm_assert(L.frame < 1 << 14);
        put(out, u16(L.frame | u16(L.pass) << 14));
        put_bbox(out, L);
    }
}

void writer::write_chunk(Array<char>& out, const chunk& c)
{
    const auto ch = c.coord();
    put(out, chunk_magic);
    put(out, ch.x);
    put(out, ch.y);
    put(out, ch.z);

    for (u32 i = 0; i < TILE_COUNT; i++)
    {
        const auto t = tile_proto(c[i]);
        if (!t.ground_atlas && !t.wall_north_atlas && !t.wall_west_atlas)
        {
            const auto max = Math::min(u32(TILE_COUNT), i + 0x80u);
            auto j = i + 1;
            while (j < max)
            {
                const auto u = tile_proto(c[j]);
                if (u.ground_atlas || u.wall_north_atlas || u.wall_west_atlas)
                    break;
                j++;
            }
            const auto skipped = j - i - 1;
            put(out, u8(meta_rle | skipped));
            i += skipped;
            continue;
        }
        put(out, u8(meta_ground * !!t.ground_atlas | meta_wall_n * !!t.wall_north_atlas |
                    meta_wall_w * !!t.wall_west_atlas));
        if (const auto& G = t.ground_atlas)
        {
            put(out, intern_atlas(&*G, G->name(), G->num_tiles2()));
            put_variant(out, t.ground_variant);
        }
        if (const auto& N = t.wall_north_atlas)
        {
            put(out, intern_atlas(&*N, N->name(), {0xff, 0xff}));
            put_variant(out, t.wall_north_variant);
        }
        if (const auto& W = t.wall_west_atlas)
        {
            put(out, intern_atlas(&*W, W->name(), {0xff, 0xff}));
            put_variant(out, t.wall_west_variant);
        }
    }

    put(out, (u32)c.objects().size());
    for (u32 k = 0; k < c.objects().size(); k++)
    {
        const auto& e = c.objects()[k];
        fm_assert(e.id < object_id{1} << collision_data_BITS);
        put(out, e.id);
        put(out, std::to_underlying(e.type()));
        put(out, e.coord.local().to_index());
        put(out, e.offset[0]);
        put(out, e.offset[1]);
        switch (e.type())
        {
        case object_type::critter: write_critter(out, static_cast<const critter&>(e)); break;
        case object_type::scenery: write_scenery(out, static_cast<const scenery&>(e)); break;
        case object_type::light: write_light(out, static_cast<const light&>(e)); break;
        case object_type::hole:
        case object_type::none:
        case object_type::COUNT:
            fm_abort("object type %d can't be stored in proto %u", (int)e.type(), (unsigned)proto);
        }
    }
}

Array<char> writer::write(world& w)
{
    Array<char> chunks;
    arrayReserve(chunks, 1 << 16);
    u32 chunk_count = 0;
    for (const auto& c : w.chunks())
    {
        write_chunk(chunks, c);
        chunk_count++;
    }
    fm_assert(chunk_count <= 0xffff);

    Array<char> out;
    arrayReserve(out, chunks.size() + 4096);
    constexpr auto magic = ".floormat.save"_s;
    arrayAppend(out, arrayView(magic.data(), magic.size()));
    put(out, proto);

    put(out, (u16)atlases.size());
    for (const auto& a : atlases)
    {
        put(out, a.num_tiles[0]);
        put(out, a.num_tiles[1]);
        put_string(out, a.name);
    }

    put(out, scenery_magic);
    put(out, (u16)sceneries.size());
    for (u32 i = 0; i < sceneries.size(); i++)
    {
        const auto name = sceneries[i].name;
        bool seen = false;
        for (u32 k = 0; k < i; k++)
            seen |= sceneries[k].name == name;
        if (seen)
            continue;
        u32 count = 0;
        for (u32 k = i; k < sceneries.size(); k++)
            count += sceneries[k].name == name;
        fm_assert(count < 0x100);
        put(out, (u8)count);
        put_string(out, name);
        for (u32 k = i; k < sceneries.size(); k++)
            if (sceneries[k].name == name)
            {
                put(out, (u16)k);
                put(out, sceneries[k].flags);
                put_frame(out, sceneries[k].frame);
            }
    }

    put(out, (u32)strings.size());
    for (const auto& str : strings)
        put_string(out, str);

    put(out, w.object_counter());
    put(out, (u16)chunk_count);
    arrayAppend(out, ArrayView<const char>{chunks});
    return out;
}

} // namespace

Array<char> write_old_savegame(world& w, ArrayView<const StringView> scenery_names)
{
    return writer{scenery_names}.write(w);
}

} // namespace floormat::Test
