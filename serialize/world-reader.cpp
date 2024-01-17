#define FM_SERIALIZE_WORLD_IMPL
#include "world-impl.hpp"
#include "binary-reader.inl"
#include "src/world.hpp"
#include "src/scenery.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "loader/loader.hpp"
#include "loader/scenery.hpp"
#include "src/ground-atlas.hpp"
#include "src/anim-atlas.hpp"
#include "src/chunk-scenery.hpp"
#include "compat/strerror.hpp"
#include <cerrno>
#include <cstring>
#include <memory>
#include <vector>

namespace {

using namespace floormat;
using namespace floormat::Serialize;

constexpr inline atlasid meta_short_scenery_bit_ = highbits<atlasid, 1, 0>;
constexpr inline atlasid meta_rotation_bits_ = highbits<atlasid, rotation_BITS, 1>;
constexpr inline atlasid scenery_id_flag_mask_ = meta_short_scenery_bit_ | meta_rotation_bits_;
constexpr inline atlasid scenery_id_max_ = int_traits<atlasid>::max & ~scenery_id_flag_mask_;

struct reader_state final {
    explicit reader_state(world& world) noexcept;
    void deserialize_world(ArrayView<const char> buf);

private:
    using reader_t = binary_reader<decltype(ArrayView<const char>{}.cbegin())>;

    StringView lookup_atlas(atlasid id);
    const scenery_proto& lookup_scenery(atlasid id);
    StringView lookup_string(uint32_t idx);
    void read_atlases(reader_t& reader);
    void read_sceneries(reader_t& reader);
    void read_strings(reader_t& reader);
    void read_chunks(reader_t& reader);
    void read_old_scenery(reader_t& s, chunk_coords_ ch, size_t i);
    void preload_chunks();

    std::vector<String> strings;
    std::vector<scenery_proto> sceneries;
    std::vector<String> atlases;
    world* _world;
    uint16_t PROTO = proto_version;

    Array<chunk::object_draw_order> draw_array;
    Array<std::array<chunk::vertex, 4>> draw_vertexes;
    Array<std::array<UnsignedShort, 6>> draw_indexes;
};

reader_state::reader_state(world& world) noexcept : _world{&world} {}

void reader_state::read_atlases(reader_t& s)
{
    const auto N = s.read<atlasid>();
    atlases.reserve(N);
    for (atlasid i = 0; i < N; i++)
    {
        Vector2ub size;
        size[0] << s;
        size[1] << s;
        const auto& [buf, len] = s.read_asciiz_string<atlas_name_max>();
        atlases.push_back({buf, len});
    }
}

template<typename T, object_subtype U>
bool read_object_flags(binary_reader<T>& s, U& e)
{
    constexpr auto tag = object_type_<U>::value;
    uint8_t flags; flags << s;
    e.pass = pass_mode(flags & pass_mask);
    if (e.type != tag)
         fm_throw("invalid object type '{}'"_cf, (int)e.type);
    if constexpr(tag == object_type::scenery)
    {
        e.active      = !!(flags & 1 << 2);
        e.closing     = !!(flags & 1 << 3);
        e.interactive = !!(flags & 1 << 4);
    }
    else if constexpr(tag == object_type::critter)
    {
        e.playable    = !!(flags & 1 << 2);
    }
    else
        static_assert(tag == object_type::none);
    return flags & 1 << 7;
}

void reader_state::read_sceneries(reader_t& s)
{
    (void)loader.sceneries();

    uint16_t magic; magic << s;
    if (magic != scenery_magic)
        fm_throw("bad scenery magic"_cf);
    atlasid sz; sz << s;
    fm_soft_assert(sz < scenery_id_max_);
    sceneries.resize(sz);

    auto i = 0uz;
    while (i < sz)
    {
        uint8_t num; num << s;
        fm_soft_assert(num > 0);
        auto str = s.read_asciiz_string<atlas_name_max>();
        auto sc = loader.scenery(str);
        for (auto n = 0uz; n < num; n++)
        {
            atlasid id; id << s;
            fm_soft_assert(id < sz);
            fm_soft_assert(!sceneries[id]);
            bool short_frame = read_object_flags(s, sc);
            fm_debug_assert(sc.atlas != nullptr);
            if (short_frame)
                sc.frame = s.read<uint8_t>();
            else
                sc.frame << s;
            fm_soft_assert(sc.frame < sc.atlas->info().nframes);
            sceneries[id] = sc;
        }
        i += num;
    }
    fm_soft_assert(i == sz);
}

void reader_state::read_strings(reader_t& s)
{
    uint32_t size; size << s;
    strings.reserve(size);
    for (auto i = 0uz; i < size; i++)
    {
        auto str = s.read_asciiz_string<string_max>();
        strings.emplace_back(StringView{str});
    }
}

StringView reader_state::lookup_atlas(atlasid id)
{
    if (id < atlases.size())
        return atlases[id];
    else
        fm_throw("no such atlas: '{}'"_cf, id);
}

const scenery_proto& reader_state::lookup_scenery(atlasid id)
{
    if (id < sceneries.size())
        return sceneries[id];
    else
        fm_throw("no such scenery: '{}'"_cf, id);
}

StringView reader_state::lookup_string(uint32_t idx)
{
    fm_soft_assert(idx < strings.size());
    return strings[idx];
}

#ifndef FM_NO_DEBUG
#   define SET_CHUNK_SIZE() do { nbytes_read = s.bytes_read() - nbytes_start; } while (false)
#else
#   define SET_CHUNK_SIZE() void()
#endif

void reader_state::read_chunks(reader_t& s)
{
    Array<typename chunk::object_draw_order> array;
    const auto N = s.read<chunksiz>();
#ifndef FM_NO_DEBUG
    [[maybe_unused]] size_t nbytes_read = 0;
#endif

    for (auto k = 0uz; k < N; k++)
    {
        const auto nbytes_start = s.bytes_read();

        std::decay_t<decltype(chunk_magic)> magic;
        magic << s;
        if (magic != chunk_magic)
            fm_throw("bad chunk magic"_cf);
        chunk_coords_ ch;
        ch.x << s;
        ch.y << s;
        if (PROTO >= 10) [[likely]]
            ch.z << s;
        auto& c = (*_world)[ch];
        c.mark_modified();
        for (auto i = 0uz; i < TILE_COUNT; i++)
        {
            SET_CHUNK_SIZE();
            const tilemeta flags = s.read<tilemeta>();

            if (PROTO >= 11) [[likely]]
                if (flags & meta_rle)
                {
                    auto j = flags & 0x7fuz;
                    i += j;
                    continue;
                }

            tile_ref t = c[i];
            using uchar = uint8_t;
            const auto make_atlas = [&]<typename T>() -> image_proto_<T> {
                atlasid id;
                if (PROTO < 8) [[unlikely]]
                    id = flags & meta_short_atlasid_ ? atlasid{s.read<uchar>()} : s.read<atlasid>();
                else
                    id << s;
                variant_t v;
                if (PROTO >= 2) [[likely]]
                    v << s;
                else
                    v = flags & meta_short_variant_
                        ? s.read<uint8_t>()
                        : uint8_t(s.read<uint16_t>());
                auto name = lookup_atlas(id);
                if constexpr(std::is_same_v<ground_atlas, T>)
                {
                    auto atlas = loader.ground_atlas(name, loader_policy::warn);
                    fm_soft_assert(v < atlas->num_tiles());
                    return { atlas, v };
                }
                else if (std::is_same_v<wall_atlas, T>)
                {
                    auto atlas = loader.wall_atlas(name, true);
                    return { atlas, v };
                }
                else
                    std::unreachable();
            };
            SET_CHUNK_SIZE();
            //t.passability() = pass_mode(flags & pass_mask);
            if (flags & meta_ground)
                t.ground() = make_atlas.operator()<ground_atlas>();
            if (flags & meta_wall_n)
                t.wall_north() = make_atlas.operator()<wall_atlas>();
            if (flags & meta_wall_w)
                t.wall_west() = make_atlas.operator()<wall_atlas>();
            if (PROTO >= 3 && PROTO < 8) [[unlikely]]
                if (flags & meta_scenery_)
                    read_old_scenery(s, ch, i);
            SET_CHUNK_SIZE();
        }
        uint32_t object_count = 0;
        if (PROTO >= 8) [[likely]]
                object_count << s;

        SET_CHUNK_SIZE();

        for (auto i = 0uz; i < object_count; i++)
        {
            object_id oid;
            object_type type;
            if (PROTO >= 18) [[likely]]
            {
                oid << s;
                fm_soft_assert((oid & lowbits<collision_data_BITS, object_id>) == oid);
                type = object_type(s.read<std::underlying_type_t<object_type>>());
                fm_soft_assert(type < object_type::COUNT);
            }
            else
            {
                object_id _id; _id << s;
                oid = _id & lowbits<60, object_id>;
                fm_soft_assert(oid != 0);
                type = object_type(_id >> 61);
            }
            const auto local = local_coords{s.read<uint8_t>()};

            Vector2b offset;
            if (PROTO >= 14) [[likely]]
            {
                offset[0] << s;
                offset[1] << s;
            }
            constexpr auto read_bbox = [](auto& s, auto& e) {
                s >> e.bbox_offset[0];
                s >> e.bbox_offset[1];
                s >> e.bbox_size[0];
                s >> e.bbox_size[1];
            };
            SET_CHUNK_SIZE();
            switch (type)
            {
            case object_type::critter: {
                critter_proto proto;
                proto.offset = offset;
                uint8_t id; id << s;
                proto.r = rotation(id >> sizeof(id)*8-1-rotation_BITS & rotation_MASK);
                if (read_object_flags(s, proto))
                    proto.frame = s.read<uint8_t>();
                else
                    proto.frame << s;
                Vector2us offset_frac;
                offset_frac[0] << s;
                offset_frac[1] << s;
                if (PROTO < 17) [[unlikely]]
                    offset_frac = {};
                const bool exact = id & meta_short_scenery_bit_;
                SET_CHUNK_SIZE();

                if (PROTO >= 9) [[likely]]
                {
                    uint32_t id; id << s;
                    auto name = lookup_string(id);
                    fm_soft_assert(name.size() < critter_name_max);
                    proto.name = name;
                }
                else
                {
                    auto [buf, len] = s.read_asciiz_string<critter_name_max>();
                    auto name = StringView{buf, len};
                    proto.name = name;
                }
                if (!exact)
                {
                    if (PROTO < 14) [[unlikely]]
                    {
                        s >> proto.offset[0];
                        s >> proto.offset[1];
                    }
                    read_bbox(s, proto);
                }
                SET_CHUNK_SIZE();
                auto e = _world->make_object<critter, false>(oid, {ch, local}, proto);
                e->offset_frac = offset_frac;
                (void)e;
                break;
            }
            case object_type::scenery: {
                atlasid id; id << s;
                bool exact;
                rotation r;
                if (PROTO >= 19) [[likely]]
                {
                    uint8_t bits; bits << s;
                    exact = bits & meta_short_scenery_bit;
                    r = rotation(bits >> sizeof(bits)*8-1-rotation_BITS & rotation_MASK);
                }
                else
                {
                    exact = id & meta_short_scenery_bit_;
                    r = rotation(id >> sizeof(id)*8-1-rotation_BITS & rotation_MASK);
                    id &= ~scenery_id_flag_mask_;
                }
                auto sc = lookup_scenery(id);
                sc.offset = offset;
                (void)sc.atlas->group(r);
                sc.r = r;
                if (!exact)
                {
                    if (read_object_flags(s, sc))
                        sc.frame = s.read<uint8_t>();
                    else
                        sc.frame << s;
                    (void)sc.atlas->frame(sc.r, sc.frame);
                    if (PROTO < 14) [[unlikely]]
                    {
                        s >> sc.offset[0];
                        s >> sc.offset[1];
                    }
                    read_bbox(s, sc);
                    if (sc.active)
                        sc.delta << s;
                }
                auto e = _world->make_object<scenery, false>(oid, {ch, local}, sc);
                (void)e;
                break;
            }
            case object_type::light: {
                light_proto proto;
                proto.offset = offset;

                uint8_t flags; flags << s;
                const bool exact = flags & 1;
                proto.r = rotation((flags >> 1) & lowbits<rotation_BITS>);
                bool enabled;
                if (PROTO >= 16) [[likely]]
                {
                    proto.falloff = light_falloff((flags >> 4) & lowbits<light_falloff_BITS>);
                    enabled = (flags >> 7) & 1;
                }
                else
                {
                    proto.falloff = light_falloff((flags >> 4) & lowbits<2>);
                    enabled = (flags >> 6) & 1;
                }
                s >> proto.max_distance;
                for (auto i = 0uz; i < 3; i++)
                    s >> proto.color[i];
                if (PROTO >= 15) [[likely]]
                    s >> proto.color[3];
                if (!exact)
                {
                    uint16_t frame; frame << s;
                    auto pass = pass_mode((frame >> 14) & lowbits<2>);
                    frame &= lowbits<14, uint16_t>;
                    proto.pass = pass;
                    proto.frame = frame;
                    read_bbox(s, proto);
                }
                SET_CHUNK_SIZE();
                auto L = _world->make_object<light, false>(oid, {ch, local}, proto);
                L->enabled = enabled;
                (void)L;
                break;
            }
            default:
                fm_throw("invalid_object_type '{}'"_cf, (int)type);
            }
        }

        SET_CHUNK_SIZE();
        fm_assert(c.is_scenery_modified());
        fm_assert(c.is_passability_modified());
        c.sort_objects();
    }
}

void reader_state::preload_chunks()
{
    for (auto& [coord, _] : _world->chunks())
    {
        auto* c = _world->at(coord);
        fm_assert(c);
        c->ensure_ground_mesh();
        c->ensure_wall_mesh();
        c->ensure_scenery_mesh({ draw_array, draw_vertexes, draw_indexes });
        c->ensure_passability();
    }
}

void reader_state::read_old_scenery(reader_t& s, chunk_coords_ ch, size_t i)
{
    atlasid id; id << s;
    const bool exact = id & meta_short_scenery_bit_;
    const auto r = rotation(id >> sizeof(id)*8-1-rotation_BITS & rotation_MASK);
    id &= ~scenery_id_flag_mask_;
    auto sc = lookup_scenery(id);
    (void)sc.atlas->group(r);
    sc.r = r;
    if (!exact)
    {
        if (read_object_flags(s, sc))
            sc.frame = s.read<uint8_t>();
        else
            sc.frame << s;
        if (PROTO >= 5) [[likely]]
        {
            sc.offset[0] << s;
            sc.offset[1] << s;
        }
        if (PROTO >= 6) [[likely]]
        {
            sc.bbox_size[0] << s;
            sc.bbox_size[1] << s;
        }
        if (PROTO >= 7) [[likely]]
        {
            sc.bbox_offset[0] << s;
            sc.bbox_offset[1] << s;
        }
        if (sc.active)
        {
            if (PROTO >= 4) [[likely]]
                sc.delta << s;
            else
                sc.delta = (uint16_t)Math::clamp(int(s.read<float>() * 65535), 0, 65535);
        }
    }
    global_coords coord{ch, local_coords{i}};
    auto e = _world->make_object<scenery, false>(_world->make_id(), coord, sc);
    (void)e;
}

void reader_state::deserialize_world(ArrayView<const char> buf)
{
    fm_assert(_world != nullptr);
    auto s = binary_reader{buf};
    if (!!::memcmp(s.read<std::size(file_magic)-1>().data(), file_magic, std::size(file_magic)-1))
        fm_throw("bad magic"_cf);
    proto_t proto;
    proto << s;
    if (!(proto >= min_proto_version && proto <= proto_version))
        fm_throw("bad proto version '{}' (should be between '{}' and '{}')"_cf,
                 (size_t)proto, (size_t)min_proto_version, (size_t)proto_version);
    PROTO = proto;
    fm_assert(PROTO > 0);
    object_id object_counter = world::object_counter_init;
    read_atlases(s);
    if (PROTO >= 3) [[likely]]
        read_sceneries(s);
    if (PROTO >= 9) [[likely]]
        read_strings(s);
    if (PROTO >= 8) [[likely]]
        object_counter << s;
    read_chunks(s);
    s.assert_end();
    if (PROTO >= 8) [[likely]]
        fm_assert(_world->object_counter() == world::object_counter_init);
    if (PROTO >= 13) [[likely]]
        _world->set_object_counter(object_counter);
    else if (PROTO >= 8) [[likely]]
        _world->set_object_counter(std::max(world::object_counter_init, object_counter));
    preload_chunks();
    _world = nullptr;
}

} // namespace

namespace floormat {

world world::deserialize(StringView filename)
{
    char errbuf[128];
    fm_soft_assert(filename.flags() & StringViewFlag::NullTerminated);
    FILE_raii f = ::fopen(filename.data(), "rb");
    if (!f)
        fm_throw("fopen(\"{}\", \"r\"): {}"_cf, filename, get_error_string(errbuf));
    if (int ret = ::fseek(f, 0, SEEK_END); ret != 0)
        fm_throw("fseek(SEEK_END): {}"_cf, get_error_string(errbuf));
    size_t len;
    if (auto len_ = ::ftell(f); len_ >= 0)
        len = (size_t)len_;
    else
        fm_throw("ftell: {}"_cf, get_error_string(errbuf));
    if (int ret = ::fseek(f, 0, SEEK_SET); ret != 0)
        fm_throw("fseek(SEEK_SET): {}"_cf, get_error_string(errbuf));
    auto buf_ = std::make_unique<char[]>(len);

    if (auto ret = ::fread(&buf_[0], 1, len, f); ret != len)
        fm_throw("fread short read: {}"_cf, get_error_string(errbuf));

    world w;
    reader_state s{w};
    s.deserialize_world({buf_.get(), len});
    return w;
}

} // namespace floormat
