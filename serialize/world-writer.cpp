#define FM_SERIALIZE_WORLD_IMPL
#include "world-impl.hpp"

#include "src/tile-atlas.hpp"
#include "binary-writer.inl"
#include "src/global-coords.hpp"
#include "src/chunk.hpp"
#include "src/world.hpp"
#include "src/emplacer.hpp"
#include "loader/loader.hpp"
#include "src/scenery.hpp"
#include "loader/scenery.hpp"
#include <vector>
#include <algorithm>
#include <cstring>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Path.h>

namespace {

using namespace floormat;
using namespace floormat::Serialize;

struct interned_atlas final {
    const tile_atlas* img;
    atlasid index;
};

struct interned_scenery {
    const serialized_scenery* s;
    atlasid index;
    static_assert(sizeof index >= sizeof scenery::frame);
};

struct scenery_pair {
    const serialized_scenery* s;
    atlasid index;
    bool exact_match;
};

struct writer_state final {
    writer_state(const world& world);
    ArrayView<const char> serialize_world();
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(writer_state);
    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(writer_state);

private:
    static constexpr inline scenery_pair null_scenery = { nullptr, null_atlas, true };

    atlasid intern_atlas(const tile_image_proto& img);
    atlasid maybe_intern_atlas(const tile_image_proto& img);
    scenery_pair intern_scenery(scenery_proto s, bool create);
    scenery_pair maybe_intern_scenery(const scenery_proto& s, bool create);

    void serialize_chunk(const chunk& c, chunk_coords coord);
    void serialize_atlases();
    void serialize_scenery();

    void load_scenery_1(const serialized_scenery& s);
    void load_scenery();

    const world* _world;
    std::vector<char> atlas_buf, scenery_buf, chunk_buf, file_buf;
    std::vector<std::vector<char>> chunk_bufs;
    std::unordered_map<const void*, interned_atlas> tile_images;
    std::unordered_map<const void*, std::vector<interned_scenery>> scenery_map;
    atlasid scenery_map_size = 0;
};

constexpr auto tile_size = sizeof(tilemeta) + (sizeof(atlasid) + sizeof(variant_t)) * 3;

constexpr auto chunkbuf_size =
        sizeof(chunk_magic) + sizeof(chunk_coords) + tile_size * TILE_COUNT;

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

writer_state::writer_state(const world& world) : _world{&world}
{
    chunk_buf.reserve(chunkbuf_size);
    chunk_bufs.reserve(world.chunks().size());
    atlas_buf.reserve(atlas_name_max * 64);
}

#ifdef __GNUG__
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning(pop)
#endif

atlasid writer_state::intern_atlas(const tile_image_proto& img)
{
    const void* const ptr = img.atlas.get();
    fm_debug_assert(ptr != nullptr);
    emplacer e{[&]() -> interned_atlas { return { &*img.atlas, (atlasid)tile_images.size() }; }};
    return tile_images.try_emplace(ptr, e).first->second.index;
}

atlasid writer_state::maybe_intern_atlas(const tile_image_proto& img)
{
    return img ? intern_atlas(img) : null_atlas;
}

void writer_state::load_scenery_1(const serialized_scenery& s)
{
    const void* const ptr = s.proto.atlas.get();
    fm_debug_assert(ptr != nullptr);
    if (auto it = scenery_map.find(ptr); it == scenery_map.end())
        scenery_map[ptr] = { { &s, null_atlas } };
    else
    {
        fm_assert(s.proto.frame.delta == 0.f);
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

scenery_pair writer_state::intern_scenery(scenery_proto s, bool create)
{
    const void* const ptr = s.atlas.get();
    fm_debug_assert(ptr != nullptr);
    auto it = scenery_map.find(ptr);
    fm_assert(it != scenery_map.end() && !it->second.empty());
    auto& vec = it->second;
    interned_scenery *ret = nullptr, *ret2 = nullptr;
    for (interned_scenery& x : vec)
    {
        fm_debug_assert(s.frame.type == x.s->proto.frame.type);
        s.frame.r = x.s->proto.frame.r;
        if (x.s->proto.frame == s.frame)
        {
            if (x.index != null_atlas)
                return { x.s, x.index, true };
            else
                ret = &x;
        }
        else if (x.index != null_atlas)
            ret2 = &x;
    }

    if (ret)
    {
        ret->index = scenery_map_size++;
        return { ret->s, ret->index, true };
    }
    else if (create)
    {
        if (ret2)
            return { ret2->s, ret2->index, false };
        else
        {
            fm_assert(!vec[0].s->proto);
            return { vec[0].s, vec[0].index = scenery_map_size++, false };
        }
    }
    else
        return {};
}

scenery_pair writer_state::maybe_intern_scenery(const scenery_proto& s, bool create)
{
    return s ? intern_scenery(s, create) : null_scenery;
}

template<typename T>
void write_scenery_flags(binary_writer<T>& s, const scenery& proto)
{
    std::uint8_t flags = 0;
    flags |= (1 << 0) * proto.passable;
    flags |= (1 << 1) * proto.blocks_view;
    flags |= (1 << 2) * proto.active;
    flags |= (1 << 3) * proto.closing;
    flags |= (1 << 4) * proto.interactive;
    flags |= (1 << 7) * (proto.frame <= 0xff);
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

    for (const auto& [atlas, _] : atlases)
    {
        const auto name = atlas->name();
        const auto namesiz = name.size();
        fm_debug_assert(s.bytes_written() + namesiz < atlasbuf_size);
        fm_assert(namesiz < atlas_name_max);
        fm_debug_assert(name.find('\0') == name.cend());
        const auto sz2 = atlas->num_tiles2();
        s << sz2[0]; s << sz2[1];
        s.write_asciiz_string(name);
    }
    atlas_buf.resize(s.bytes_written());
    fm_assert(s.bytes_written() <= atlasbuf_size);
}

constexpr auto atlasbuf_size0 = sizeof(atlasid) + sizeof(scenery);
constexpr auto atlasbuf_size1 = sizeof(std::uint8_t) + atlasbuf_size0*int_max<std::uint8_t> + atlas_name_max;

void writer_state::serialize_scenery()
{
    fm_assert(scenery_map_size < scenery_id_max);
    const std::size_t sz = scenery_map_size;
    std::vector<interned_scenery> vec; vec.reserve(scenery_map_size);
    for (const auto& x : scenery_map)
        for (const auto& s : x.second)
            if (s.index != null_atlas)
                vec.push_back(s);
    fm_assert(sz == vec.size());

    std::sort(vec.begin(), vec.end(), [](const interned_scenery& a, const interned_scenery& b) {
      auto cmp = a.s->name <=> b.s->name;
      if (cmp == std::strong_ordering::equal)
          return a.index < b.index;
      else
          return cmp == std::strong_ordering::less;
    });

    const auto atlasbuf_size = sizeof(std::uint16_t) + sizeof(sz) + atlasbuf_size1*sz;
    scenery_buf.resize(atlasbuf_size);

    auto s = binary_writer{scenery_buf.begin()};

    s << std::uint16_t{scenery_magic};
    fm_assert(sz < scenery_id_max);
    s << (atlasid)sz;

    StringView last;
    for (std::size_t i = 0; i < sz; i++)
    {
        fm_debug_assert(s.bytes_written() + atlasbuf_size1 < atlasbuf_size);
        const auto& [sc, idx] = vec[i];
        if (sc->name != last)
        {
            fm_assert(sc->name.size() < atlas_name_max);
            last = sc->name;
            std::size_t num = 1;
            for (std::size_t j = i+1; j < sz && vec[j].s->name == sc->name; j++)
                num++;
            fm_assert(num < int_max<std::uint8_t>);
            s << (std::uint8_t)num;
            s.write_asciiz_string(sc->name);
        }
        s << idx;
        const auto& fr = sc->proto.frame;
        write_scenery_flags(s, sc->proto.frame);
        if (sc->proto.frame.frame <= 0xff)
            s << (std::uint8_t)fr.frame;
        else
            s << fr.frame;
    }

    scenery_buf.resize(s.bytes_written());
}

void writer_state::serialize_chunk(const chunk& c, chunk_coords coord)
{
    fm_assert(chunk_buf.empty());
    chunk_buf.resize(chunkbuf_size);

    auto s = binary_writer{chunk_buf.begin()};

    s << chunk_magic << coord.x << coord.y;

    for (std::size_t i = 0; i < TILE_COUNT; i++)
    {
        const tile_proto x = c[i];
        const auto ground = x.ground(), wall_north = x.wall_north(), wall_west = x.wall_west();
        const auto scenery = x.scenery_frame;

        fm_debug_assert(s.bytes_written() + tile_size <= chunkbuf_size);

        auto img_g = maybe_intern_atlas(ground);
        auto img_n = maybe_intern_atlas(wall_north);
        auto img_w = maybe_intern_atlas(wall_west);
        auto [sc, img_s, sc_exact] = maybe_intern_scenery(x.scenery(), true);

        tilemeta flags = {};
        flags |= meta_ground  * (img_g != null_atlas);
        flags |= meta_wall_n  * (img_n != null_atlas);
        flags |= meta_wall_w  * (img_w != null_atlas);
        flags |= meta_scenery * (img_s != null_atlas);

        using uchar = std::uint8_t;

        constexpr auto ashortp = [](atlasid id) {
            return id == null_atlas || id == (uchar)id;
        };

        if (flags != 0 && ashortp(img_g) && ashortp(img_n) && ashortp(img_w))
            flags |= meta_short_atlasid;

        fm_debug_assert((x.pass_mode & pass_mask) == x.pass_mode);
        flags |= x.pass_mode;

        s << flags;

#ifndef FM_NO_DEBUG
        constexpr auto check_atlas = [](const tile_image_proto& x) {
            if (x.atlas)
                fm_assert(x.variant < x.atlas->num_tiles());
        };
        check_atlas(ground);
        check_atlas(wall_north);
        check_atlas(wall_west);
#endif

        const auto write = [&](atlasid x, variant_t v) {
            flags & meta_short_atlasid ? s << (uchar) x : s << x;
            s << v;
        };

        if (img_g != null_atlas)
            write(img_g, ground.variant);
        if (img_n != null_atlas)
            write(img_n, wall_north.variant);
        if (img_w != null_atlas)
            write(img_w, wall_west.variant);
        if (img_s != null_atlas)
        {
            atlasid id = img_s;
            id |= meta_long_scenery_bit * sc_exact;
            id |= atlasid(scenery.r) << sizeof(atlasid)*8-1-rotation_BITS;
            s << id;
            if (!sc_exact)
            {
                fm_assert(scenery.active || scenery.delta == 0.0f);
                write_scenery_flags(s, scenery);
                if (scenery.frame <= 0xff)
                    s << (std::uint8_t)scenery.frame;
                else
                    s << scenery.frame;
                if (scenery.active)
                    s << scenery.delta;
            }
        }
    }

    const auto nbytes = s.bytes_written();
    fm_assert(nbytes <= chunkbuf_size);

    chunk_buf.resize(nbytes);
    chunk_bufs.push_back(std::move(chunk_buf));
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
    load_scenery();

    for (const auto& [_, c] : _world->chunks())
        for (auto [x, _k, _pt] : c)
            maybe_intern_scenery(x.scenery(), false);

    for (const auto& [pos, c] : _world->chunks())
    {
#ifndef FM_NO_DEBUG
        if (c.empty(true))
            fm_warn("chunk %hd:%hd is empty", pos.x, pos.y);
#endif
        serialize_chunk(c, pos);
    }
    serialize_atlases();
    serialize_scenery();

    using proto_t = std::decay_t<decltype(proto_version)>;
    union { chunksiz x; char bytes[sizeof x]; } c = {.x = maybe_byteswap((chunksiz)_world->size())};
    union { proto_t x;  char bytes[sizeof x]; } p = {.x = maybe_byteswap(proto_version)};
    fm_assert(_world->size() <= int_max<chunksiz>);

    std::size_t len = 0;
    len += std::size(file_magic)-1;
    len += sizeof(p.x);
    len += sizeof(c.x);
    for (const auto& buf : chunk_bufs)
        len += buf.size();
    len += atlas_buf.size();
    len += scenery_buf.size();
    file_buf.resize(len);
    auto it = file_buf.begin();
    const auto copy = [&](const auto& in) {
        auto len1 = std::distance(std::cbegin(in), std::cend(in)),
             len2 = std::distance(it, file_buf.end());
        fm_assert(len1 <= len2);
        it = std::copy(std::cbegin(in), std::cend(in), it);
    };
    copy(Containers::StringView{file_magic, std::size(file_magic)-1});
    copy(p.bytes);
    copy(atlas_buf);
    copy(scenery_buf);
    copy(c.bytes);
    for (const auto& buf : chunk_bufs)
        copy(buf);
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
    constexpr auto get_error_string = []<std::size_t N> (char (&buf)[N]) {
        buf[0] = '\0';
#ifndef _WIN32
        (void)::strerror_r(errno, buf, std::size(buf));
#else
        (void)::strerror_s(buf, std::size(buf), errno);
#endif
    };
    fm_assert(filename.flags() & StringViewFlag::NullTerminated);
    if (Path::exists(filename))
        Path::remove(filename);
    FILE_raii file = ::fopen(filename.data(), "wb");
    if (!file)
    {
        get_error_string(errbuf);
        fm_abort("fopen(\"%s\", \"w\"): %s", filename.data(), errbuf);
    }
    writer_state s{*this};
    const auto array = s.serialize_world();
    if (auto len = ::fwrite(array.data(), array.size(), 1, file); len != 1)
    {
        get_error_string(errbuf);
        fm_abort("fwrite: %s", errbuf);
    }
    if (int ret = ::fflush(file); ret != 0)
    {
        get_error_string(errbuf);
        fm_abort("fflush: %s", errbuf);
    }
}

} // namespace floormat
