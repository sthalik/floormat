#define FM_SERIALIZE_WORLD_IMPL
#include "world-impl.hpp"

#include "src/tile-atlas.hpp"
#include "binary-writer.inl"
#include "src/global-coords.hpp"
#include "src/chunk.hpp"
#include "src/world.hpp"
#include <vector>
#include <algorithm>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Path.h>

namespace Path = Corrade::Utility::Path;

namespace floormat::Serialize {

namespace {

struct interned_atlas final {
    const tile_atlas* img;
    atlasid index;
};

struct writer_state final {
    writer_state(const world& world);
    ArrayView<const char> serialize_world();
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(writer_state);
    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(writer_state);

private:
    atlasid intern_atlas(const tile_image_proto& img);
    atlasid maybe_intern_atlas(const tile_image_proto& img);
    void serialize_chunk(const chunk& c, chunk_coords coord);
    void serialize_atlases();

    const struct world* world;
    std::vector<char> atlas_buf, chunk_buf, file_buf;
    std::vector<std::vector<char>> chunk_bufs;
    std::unordered_map<const void*, interned_atlas> tile_images;
};

constexpr auto tile_size = sizeof(tilemeta) + (sizeof(atlasid) + sizeof(varid)) * 3;

constexpr auto chunkbuf_size =
        sizeof(chunk_magic) + sizeof(chunk_coords) + tile_size * TILE_COUNT;

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

writer_state::writer_state(const struct world& world) : world{&world}
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
    if (auto it = tile_images.find(ptr); it != tile_images.end())
        return it->second.index;
    else
        return (tile_images[ptr] = { &*img.atlas, (atlasid)tile_images.size() }).index;
}

atlasid writer_state::maybe_intern_atlas(const tile_image_proto& img)
{
    return img ? intern_atlas(img) : null_atlas;
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
        const auto ground = x.ground_image(), wall_north = x.wall_north_image(), wall_west = x.wall_west_image();

        fm_debug_assert(s.bytes_written() + tile_size <= chunkbuf_size);

        auto img_g = maybe_intern_atlas(ground);
        auto img_n = maybe_intern_atlas(wall_north);
        auto img_w = maybe_intern_atlas(wall_west);

        tilemeta flags = {};
        flags |= meta_ground * (img_g != null_atlas);
        flags |= meta_wall_n * (img_n != null_atlas);
        flags |= meta_wall_w * (img_w != null_atlas);

        using uchar = std::uint8_t;

        constexpr auto ashortp = [](atlasid id) {
            return id == null_atlas || id == (uchar)id;
        };
        constexpr auto vshortp = [](const tile_image_proto& img) {
            return !img.atlas || img.variant == (uchar)img.variant;
        };

        if (flags != 0 && ashortp(img_g) && ashortp(img_n) && ashortp(img_w))
            flags |= meta_short_atlasid;
        if (flags != 0 && vshortp(ground) && vshortp(wall_north) && vshortp(wall_west))
            flags |= meta_short_variant;

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

        const auto write = [&](atlasid x, varid v) {
            flags & meta_short_atlasid ? s << (uchar) x : s << x;
            flags & meta_short_variant ? s << (uchar) v : s << v;
        };

        if (img_g != null_atlas)
            write(img_g, ground.variant);
        if (img_n != null_atlas)
            write(img_n, wall_north.variant);
        if (img_w != null_atlas)
            write(img_w, wall_west.variant);
    }

    const auto nbytes = s.bytes_written();
    fm_assert(nbytes <= chunkbuf_size);

    chunk_buf.resize(nbytes);
    chunk_bufs.push_back(chunk_buf);
    chunk_buf.clear();
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
        fm_debug_assert(s.bytes_written() + namesiz + 1 <= atlasbuf_size);
        fm_assert(namesiz <= atlas_name_max - 1); // null terminated
        fm_debug_assert(name.find('\0') == name.cend());
        const auto sz2 = atlas->num_tiles2();
        s << sz2[0]; s << sz2[1];
        s.write_asciiz_string(name);
    }
    atlas_buf.resize(s.bytes_written());
    fm_assert(s.bytes_written() <= atlasbuf_size);
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
    for (const auto& [pos, c] : world->chunks())
    {
#ifndef FM_NO_DEBUG
        if (c.empty(true))
            fm_warn("chunk %hd:%hd is empty", pos.x, pos.y);
#endif
        serialize_chunk(c, pos);
    }
    serialize_atlases();

    using proto_t = std::decay_t<decltype(proto_version)>;
    union { chunksiz x; char bytes[sizeof x]; } c = {.x = maybe_byteswap((chunksiz)world->size())};
    union { proto_t x;  char bytes[sizeof x]; } p = {.x = maybe_byteswap(proto_version)};
    fm_assert(world->size() <= int_max<chunksiz>);

    std::size_t len = 0;
    len += std::size(file_magic)-1;
    len += sizeof(p.x);
    len += sizeof(c.x);
    for (const auto& buf : chunk_bufs)
        len += buf.size();
    len += atlas_buf.size();
    file_buf.resize(len);
    auto it = file_buf.begin();
    const auto copy = [&](const auto& in) {
#ifndef FM_NO_DEBUG
        auto len1 = std::distance(std::cbegin(in), std::cend(in)),
             len2 = std::distance(it, file_buf.end());
        fm_assert(len1 <= len2);
#endif
        it = std::copy(std::cbegin(in), std::cend(in), it);
    };
    copy(Containers::StringView{file_magic, std::size(file_magic)-1});
    copy(p.bytes);
    copy(atlas_buf);
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

} // namespace floormat::Serialize

namespace floormat {

void world::serialize(StringView filename)
{
    collect(true);
    char errbuf[128];
    constexpr auto strerror = []<std::size_t N> (char (&buf)[N]) -> const char* {
        ::strerror_s(buf, std::size(buf), errno);
        return buf;
    };
    fm_assert(filename.flags() & StringViewFlag::NullTerminated);
    if (Path::exists(filename))
        Path::remove(filename);
    FILE_raii file = ::fopen(filename.data(), "wb");
    if (!file)
        fm_abort("fopen(\"%s\", \"w\"): %s", filename.data(), strerror(errbuf));
    Serialize::writer_state s{*this};
    const auto array = s.serialize_world();
    if (auto len = ::fwrite(array.data(), array.size(), 1, file); len != 1)
        fm_abort("fwrite: %s", strerror(errbuf));
    if (int ret = ::fflush(file); ret != 0)
        fm_abort("fflush: %s", strerror(errbuf));
}

} // namespace floormat
