#define FM_SERIALIZE_WORLD_IMPL
#include "world-impl.hpp"
#include "src/tile-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "binary-writer.inl"
#include "src/global-coords.hpp"
#include "src/chunk.hpp"
#include "src/world.hpp"
#include "src/emplacer.hpp"
#include "loader/loader.hpp"
#include "src/scenery.hpp"
#include "src/critter.hpp"
#include "loader/scenery.hpp"
#include "src/anim-atlas.hpp"
#include "src/light.hpp"
#include "compat/strerror.hpp"
#include <cerrno>
#include <cstring>
#include <concepts>
#include <vector>
#include <algorithm>
#include <string_view>
#include <tsl/robin_map.h>
#include <Corrade/Containers/StringStlHash.h>
#include <Corrade/Utility/Path.h>

using namespace floormat;
using namespace floormat::Serialize;

namespace {

struct interned_atlas final {
    StringView name;
    atlasid index;
    Vector2ub num_tiles;
};

struct interned_scenery {
    const serialized_scenery* s;
    atlasid index;
    static_assert(sizeof index >= sizeof scenery::frame);
};

struct scenery_pair {
    const scenery_proto* s;
    atlasid index;
    bool exact_match;
};

struct writer_state final {
    writer_state(const world& world);
    ArrayView<const char> serialize_world();
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(writer_state);
    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(writer_state);

private:
    using writer_t = binary_writer<decltype(std::vector<char>{}.begin())>;

    atlasid intern_atlas(const void* ptr, StringView name, Vector2ub num_tiles);
    scenery_pair intern_scenery(const scenery& sc, bool create);
    uint32_t intern_string(StringView name);

    void serialize_scenery(const chunk& c, writer_t& s);
    void serialize_chunk(const chunk& c, chunk_coords_ coord);
    void serialize_atlases();
    void serialize_scenery_names();
    void serialize_strings();

    void load_scenery_1(const serialized_scenery& s);
    void load_scenery();

    const world* _world;
    std::vector<char> atlas_buf, scenery_buf, chunk_buf, file_buf, string_buf;
    std::vector<std::vector<char>> chunk_bufs;
    tsl::robin_map<const void*, interned_atlas> tile_images;
    std::unordered_map<const void*, std::vector<interned_scenery>> scenery_map;
    tsl::robin_map<StringView, uint32_t> string_map;
    atlasid scenery_map_size = 0;
};

constexpr auto tile_size = sizeof(tilemeta) + (sizeof(atlasid) + sizeof(variant_t)) * 3;
constexpr auto chunkbuf_size = sizeof(chunk_magic) + sizeof(chunk_coords_) + tile_size * TILE_COUNT + sizeof(uint32_t);
constexpr auto object_size = std::max({ sizeof(critter), sizeof(scenery), sizeof(light), });

writer_state::writer_state(const world& world) : _world{&world}
{
    chunk_buf.reserve(chunkbuf_size);
    chunk_bufs.reserve(world.chunks().size());
    atlas_buf.reserve(atlas_name_max * 64);
    scenery_map.reserve(64);
    string_map.reserve(64);
}

uint32_t writer_state::intern_string(StringView name)
{
    auto [kv, fresh] = string_map.try_emplace(name, (uint32_t)string_map.size());
    return kv->second;
}

atlasid writer_state::intern_atlas(const void* ptr, StringView name, Vector2ub num_tiles)
{
    fm_debug_assert(ptr != nullptr);
    if (auto it = tile_images.find(ptr); it != tile_images.end())
        return it->second.index;
    else
    {
        auto aid = (atlasid)tile_images.size();
        tile_images[ptr] = { name, aid, num_tiles };
        return aid;
    }
}

void writer_state::load_scenery_1(const serialized_scenery& s)
{
    const void* const ptr = s.proto.atlas.get();
    fm_debug_assert(ptr != nullptr);
    if (auto it = scenery_map.find(ptr); it == scenery_map.end())
        scenery_map[ptr] = { { &s, null_atlas } };
    else
    {
        auto& vec = scenery_map[ptr];
        for (const auto& x : vec)
            if (s.proto.frame == x.s->proto.frame)
                return;
        vec.push_back({ &s, null_atlas });
    }
}

void writer_state::load_scenery()
{
    for (const auto& s : loader.sceneries())
        load_scenery_1(s);
}

scenery_pair writer_state::intern_scenery(const scenery& sc, bool create)
{
    auto s = scenery_proto(sc);
    const void* const ptr = s.atlas.get();
    fm_debug_assert(ptr != nullptr);
    auto it = scenery_map.find(ptr);
    fm_assert(it != scenery_map.end() && !it->second.empty());
    auto& vec = it->second;
    interned_scenery *ret = nullptr, *ret2 = nullptr;
    for (interned_scenery& x : vec)
    {
        const auto& proto = x.s->proto;
        fm_assert(s.type == proto.type);
        fm_assert(s.sc_type == proto.sc_type);
        s.r = proto.r;
        s.interactive = proto.interactive;
        s.active  = proto.active;
        s.closing = proto.closing;
        s.pass = proto.pass;
        if (s == proto)
        {
            if (x.index != null_atlas)
                return { &x.s->proto, x.index, true };
            else
                ret = &x;
        }
        if (x.index != null_atlas)
            ret2 = &x;
    }

    if (ret)
    {
        ret->index = scenery_map_size++;
        return { &ret->s->proto, ret->index, true };
    }
    else if (create)
    {
        if (ret2)
            return { &ret2->s->proto, ret2->index, false };
        else
        {
            fm_assert(vec[0].index == null_atlas);
            return { &vec[0].s->proto, vec[0].index = scenery_map_size++, false };
        }
    }
    else
        return {};
}

template<typename T, object_subtype U>
void write_object_flags(binary_writer<T>& s, const U& e)
{
    uint8_t flags = 0;
    auto pass = (pass_mode_i)e.pass;
    fm_assert((pass & pass_mask) == pass);
    flags |= pass;
    constexpr auto tag = object_type_<U>::value;
    if (e.type_of() != tag)
        fm_abort("invalid object type '%d'", (int)e.type_of());
    if constexpr(tag == object_type::scenery)
    {
        flags |= (1 << 2) * e.active;
        flags |= (1 << 3) * e.closing;
        flags |= (1 << 4) * e.interactive;
    }
    else if constexpr(tag == object_type::critter)
    {
        flags |= (1 << 2) * e.playable;
    }
    else
        static_assert(tag == object_type::none);
    flags |= (1 << 7) * (e.frame <= 0xff);
    s << flags;
}

void writer_state::serialize_atlases()
{
    fm_assert(tile_images.size() < int_max<atlasid>);
    const auto sz = (atlasid)tile_images.size();
    const auto atlasbuf_size = sizeof(sz) + atlas_name_max*sz;
    atlas_buf.resize(atlasbuf_size);
    auto s = binary_writer{atlas_buf.begin()};
    fm_assert(sz <= int_max<atlasid>);

    s << sz;

    std::vector<interned_atlas> atlases;
    atlases.reserve(tile_images.size());

    for (const auto& [_, t] : tile_images)
        atlases.push_back(t);
    std::sort(atlases.begin(), atlases.end(), [](const auto& a, const auto& b) {
        return a.index < b.index;
    });

    for (const auto& [name, aid, num_tiles] : atlases)
    {
        const auto namesiz = name.size();
        fm_debug_assert(s.bytes_written() + namesiz < atlasbuf_size);
        fm_assert(namesiz < atlas_name_max);
        s << num_tiles[0]; s << num_tiles[1];
        s.write_asciiz_string(name);
    }
    atlas_buf.resize(s.bytes_written());
    fm_assert(s.bytes_written() <= atlasbuf_size);
}

constexpr auto atlasbuf_size0 = sizeof(atlasid) + sizeof(scenery);
constexpr auto atlasbuf_size1 = sizeof(uint8_t) + atlasbuf_size0*int_max<uint8_t> + atlas_name_max;

void writer_state::serialize_scenery_names()
{
    fm_assert(scenery_map_size < scenery_id_max);
    const size_t sz = scenery_map_size;
    std::vector<interned_scenery> vec; vec.reserve(scenery_map_size);
    for (const auto& x : scenery_map)
        for (const auto& s : x.second)
            if (s.index != null_atlas)
                vec.push_back(s);
    fm_assert(sz == vec.size());

    std::sort(vec.begin(), vec.end(), [](const interned_scenery& a, const interned_scenery& b) {
      auto a_ = std::string_view{a.s->name.data(), a.s->name.size()},
           b_ = std::string_view{b.s->name.data(), b.s->name.size()};
      auto cmp = a_ <=> b_;
      if (cmp == std::strong_ordering::equal)
          return a.index < b.index;
      else
          return cmp == std::strong_ordering::less;
    });

    const auto atlasbuf_size = sizeof(uint16_t) + sizeof(sz) + atlasbuf_size1*sz;
    scenery_buf.resize(atlasbuf_size);

    auto s = binary_writer{scenery_buf.begin()};

    s << uint16_t{scenery_magic};
    fm_assert(sz < scenery_id_max);
    s << (atlasid)sz;

    StringView last;
    for (auto i = 0uz; i < sz; i++)
    {
        fm_debug_assert(s.bytes_written() + atlasbuf_size1 < atlasbuf_size);
        const auto& [sc, idx] = vec[i];
        if (sc->name != last)
        {
            fm_assert(sc->name.size() < atlas_name_max);
            last = sc->name;
            auto num = 1uz;
            for (auto j = i+1; j < sz && vec[j].s->name == sc->name; j++)
                num++;
            fm_assert(num < int_max<uint8_t>);
            s << (uint8_t)num;
            fm_assert(sc->name.size() < atlas_name_max);
            s.write_asciiz_string(sc->name);
        }
        s << idx;
        write_object_flags(s, sc->proto);
        if (sc->proto.frame <= 0xff)
            s << (uint8_t)sc->proto.frame;
        else
            s << sc->proto.frame;
    }

    scenery_buf.resize(s.bytes_written());
}

void writer_state::serialize_strings()
{
    static_assert(critter_name_max <= string_max);
    auto len = 0uz;
    for (const auto& [k, v] : string_map)
    {
        fm_assert(k.size()+1 < string_max);
        len += k.size()+1;
    }
    string_buf.resize(sizeof(uint32_t) + len);
    auto s = binary_writer{string_buf.begin()};
    s << (uint32_t)string_map.size();
    for (const auto& [k, v] : string_map)
    {
        fm_assert(k.size() < string_max);
        s.write_asciiz_string(k);
    }
    fm_assert(s.bytes_written() == sizeof(uint32_t) + len);
    fm_assert(s.bytes_written() == string_buf.size());
}

void writer_state::serialize_scenery(const chunk& c, writer_t& s)
{
    constexpr auto def_char_bbox_size = Vector2ub(iTILE_SIZE2); // copied from character_proto

    const auto object_count = (uint32_t)c.objects().size();
    s << object_count;
    fm_assert(object_count == c.objects().size());
    for (const auto& e_ : c.objects())
    {
        const auto& e = *e_;
        fm_assert(s.bytes_written() + object_size <= chunk_buf.size());
        object_id oid = e.id;
        fm_assert((oid & lowbits<60, object_id>) == e.id);
        const auto type = e.type();
        const auto type_ = (object_type_i)type;
        fm_assert(type_ == (type_ & lowbits<object_type_BITS, object_type_i>));
        oid |= (object_id)type << 64 - object_type_BITS;
        s << oid;
        const auto local = e.coord.local();
        s << local.to_index();
        s << e.offset[0];
        s << e.offset[1];

        constexpr auto write_bbox = [](auto& s, const auto& e) {
            s << e.bbox_offset[0];
            s << e.bbox_offset[1];
            s << e.bbox_size[0];
            s << e.bbox_size[1];
        };
        switch (type)
        {
        default:
            fm_abort("invalid object type '%d'", (int)type);
        case object_type::critter: {
            const auto& C = static_cast<const critter&>(e);
            uint8_t id = 0;
            const auto sc_exact =
                C.bbox_offset.isZero() &&
                C.bbox_size == def_char_bbox_size;
            id |= meta_short_scenery_bit * sc_exact;
            id |= static_cast<decltype(id)>(C.r) << sizeof(id)*8-1-rotation_BITS;
            s << id;
            write_object_flags(s, C);
            if (C.frame <= 0xff)
                s << (uint8_t)C.frame;
            else
                s << C.frame;
            s << C.offset_frac[0];
            s << C.offset_frac[1];
            fm_assert(C.name.size() < critter_name_max);
            s << intern_string(C.name);
            if (!sc_exact)
                write_bbox(s, C);
            break;
        }
        case object_type::scenery: {
            const auto& sc = static_cast<const scenery&>(e);
            auto [ss, img_s, sc_exact] = intern_scenery(sc, true);
            sc_exact = sc_exact &&
                       sc.bbox_offset == ss->bbox_offset && sc.bbox_size == ss->bbox_size &&
                       sc.pass == ss->pass && sc.sc_type == ss->sc_type &&
                       sc.active == ss->active && sc.closing == ss->closing &&
                       sc.interactive == ss->interactive &&
                       sc.delta == 0 && sc.frame == ss->frame;
            fm_assert(img_s != null_atlas);
            atlasid id = img_s;
            static_assert(rotation_BITS == 3);
            fm_assert(id == (id & lowbits<16-3-1, atlasid>));
            id |= meta_short_scenery_bit * sc_exact;
            id |= static_cast<decltype(id)>(sc.r) << sizeof(id)*8-1-rotation_BITS;
            s << id;
            if (!sc_exact)
            {
                write_object_flags(s, sc);
                fm_assert(sc.active || sc.delta == 0);
                if (sc.frame <= 0xff)
                    s << (uint8_t)sc.frame;
                else
                    s << sc.frame;
                write_bbox(s, sc);
                if (sc.active)
                    s << sc.delta;
            }
            break;
        }
        case object_type::light: {
            const auto& L = static_cast<const light&>(e);
            const auto exact = L.frame == 0 && L.pass == pass_mode::pass &&
                               L.bbox_offset.isZero() && L.bbox_size.isZero();
            {
                fm_assert(L.r < rotation_COUNT);
                fm_assert(L.falloff < light_falloff_COUNT);
                uint8_t flags = 0;
                flags |= (uint8_t)exact;                                            // 1 bit
                flags |= ((uint8_t)L.r       & lowbits<rotation_BITS>)      << 1;   // 3 bits
                flags |= ((uint8_t)L.falloff & lowbits<light_falloff_BITS>) << 4;   // 2 bits
                flags |= (uint8_t)!!L.enabled                               << 7;   // 1 bit
                s << flags;
            }
            {
                s << L.max_distance;
                for (auto i = 0uz; i < 4; i++)
                    s << L.color[i];
            }
            if (!exact)
            {
                fm_assert(L.frame < (1 << 14));
                fm_assert(L.pass < pass_mode_COUNT);
                uint16_t frame = 0;
                frame |= L.frame;
                frame |= uint16_t(L.pass) << 14;
                s << frame;
                write_bbox(s, L);
            }
            break;
        }
        }
    }
}

void writer_state::serialize_chunk(const chunk& c, chunk_coords_ coord)
{
    fm_assert(chunk_buf.empty());
    const auto es_size = sizeof(uint32_t) + object_size*c.objects().size();
    chunk_buf.resize(std::max(chunk_buf.size(), chunkbuf_size + es_size));

    auto s = binary_writer{chunk_buf.begin()};

    s << chunk_magic << coord.x << coord.y;
    fm_assert(coord.z >= chunk_z_min && coord.z <= chunk_z_max);
    s << coord.z;

    for (auto i = 0uz; i < TILE_COUNT; i++)
    {
        const tile_proto x = c[i];
        const auto ground = x.ground();
        const auto wall_north = x.wall_north(), wall_west = x.wall_west();
        //const auto scenery = x.scenery_frame;

        fm_debug_assert(s.bytes_written() + tile_size <= chunkbuf_size);

        auto img_g = ground.atlas ? intern_atlas(&*ground.atlas, ground.atlas->name(), ground.atlas->num_tiles2()) : null_atlas;
        auto img_n = wall_north   ? intern_atlas(&*wall_north.atlas, wall_north.atlas->name(), {0xff, 0xff}) : null_atlas;
        auto img_w = wall_west    ? intern_atlas(&*wall_west.atlas, wall_west.atlas->name(), {0xff, 0xff}) : null_atlas;

        fm_assert(!ground.atlas || ground.variant < ground.atlas->num_tiles());

        if (img_g == null_atlas && img_n == null_atlas && img_w == null_atlas)
        {
            size_t j, max = std::min(TILE_COUNT, i + 0x80);
            for (j = i+1; j < max; j++)
            {
                auto tile = c[j];
                if (tile.ground_atlas || tile.wall_north_atlas || tile.wall_west_atlas)
                    break;
            }
            j -= i + 1;
            fm_assert(j == (j & 0x7fuz));
            i += j;
            tilemeta flags = meta_rle | (tilemeta)j;
            s << flags;

            continue;
        }

        tilemeta flags = {};
        flags |= meta_ground  * (img_g != null_atlas);
        flags |= meta_wall_n  * (img_n != null_atlas);
        flags |= meta_wall_w  * (img_w != null_atlas);
        s << flags;

        if (img_g != null_atlas)
        {
            s << img_g;
            s << ground.variant;
        }
        if (img_n != null_atlas)
        {
            s << img_n;
            s << wall_north.variant;
        }
        if (img_w != null_atlas)
        {
            s << img_w;
            s << wall_west.variant;
        }
    }

    serialize_scenery(c, s);

    const auto nbytes = s.bytes_written();
    fm_assert(nbytes <= chunkbuf_size);

    chunk_bufs.emplace_back(chunk_buf.cbegin(), chunk_buf.cbegin() + ptrdiff_t(nbytes));
    chunk_buf.clear();
}

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

ArrayView<const char> writer_state::serialize_world()
{
    fm_assert(_world != nullptr);
    load_scenery();

    for (const auto& [_, c] : _world->chunks())
    {
        for (const auto& e_ : c.objects())
        {
            const auto& e = *e_;
            switch (e.type())
            {
            case object_type::scenery:
                intern_scenery(static_cast<const scenery&>(e), false);
                break;
            case object_type::critter:
            case object_type::light:
                break;
            default:
                fm_abort("invalid scenery type '%d'", (int)e.type());
            }
        }
    }
    for (const auto& [pos, c] : _world->chunks())
    {
#ifndef FM_NO_DEBUG
        if (c.empty(true))
            fm_warn("chunk %hd:%hd is empty", pos.x, pos.y);
#endif
        serialize_chunk(c, pos);
    }
    serialize_atlases();
    serialize_scenery_names();
    serialize_strings();

    using proto_t = std::decay_t<decltype(proto_version)>;
    fm_assert(_world->size() <= int_max<chunksiz>);

    const auto len = fm_begin(
        auto len = 0uz;
        len += std::size(file_magic)-1;
        len += sizeof(proto_t);
        len += atlas_buf.size();
        len += scenery_buf.size();
        len += string_buf.size();
        len += sizeof(object_id);
        len += sizeof(chunksiz);
        for (const auto& buf : chunk_bufs)
            len += buf.size();
        return len;
    );
    file_buf.resize(len);
    auto bytes_written = 0uz;
    auto it = file_buf.begin();
    const auto copy = [&](const auto& in) {
        auto len1 = std::distance(std::cbegin(in), std::cend(in)),
             len2 = std::distance(it, file_buf.end());
        fm_assert(len1 <= len2);
        it = std::copy(std::cbegin(in), std::cend(in), it);
        bytes_written += (size_t)len1;
    };
    const auto copy_int = [&]<typename T>(const T& value) {
        union { T x; char bytes[sizeof x]; } c = {.x = maybe_byteswap(value)};
        copy(c.bytes);
    };
    copy(Containers::StringView{file_magic, std::size(file_magic)-1});
    copy_int((proto_t)proto_version);
    copy(atlas_buf);
    copy(scenery_buf);
    copy(string_buf);
    copy_int(_world->object_counter());
    copy_int((chunksiz)_world->size());
    for (const auto& buf : chunk_bufs)
        copy(buf);
    fm_assert(file_buf.size() == bytes_written);
    fm_assert(len == bytes_written);
    _world = nullptr;
    return {file_buf.data(), file_buf.size()};
}

#ifdef __GNUG__
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning(pop)
#endif

} // namespace

namespace floormat {

void world::serialize(StringView filename)
{
    collect(true);
    char errbuf[128];
    fm_assert(filename.flags() & StringViewFlag::NullTerminated);
    if (Path::exists(filename))
        Path::remove(filename);
    FILE_raii file = ::fopen(filename.data(), "wb");
    if (!file)
    {
        int error = errno;
        fm_abort("fopen(\"%s\", \"w\"): %s", filename.data(), get_error_string(errbuf, error).data());
    }
    writer_state s{*this};
    const auto array = s.serialize_world();
    if (auto len = ::fwrite(array.data(), array.size(), 1, file); len != 1)
    {
        int error = errno;
        fm_abort("fwrite: %s", get_error_string(errbuf, error).data());
    }
    if (int ret = ::fflush(file); ret != 0)
    {
        int error = errno;
        fm_abort("fflush: %s", get_error_string(errbuf, error).data());
    }
}

} // namespace floormat
