#define FM_SERIALIZE_WORLD_IMPL
#include "world-impl.hpp"
#include "binary-reader.inl"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "src/tile-atlas.hpp"
#include <cstring>

namespace floormat::Serialize {

struct reader_state final {
    explicit reader_state(world& world) noexcept;
    void deserialize_world(ArrayView<const char> buf);

private:
    using reader_t = binary_reader<decltype(ArrayView<const char>{}.cbegin())>;

    std::shared_ptr<tile_atlas> lookup_atlas(atlasid id);
    void read_atlases(reader_t& reader);
    void read_chunks(reader_t& reader);

    std::unordered_map<atlasid, std::shared_ptr<tile_atlas>> atlases;
    world* _world;
};

reader_state::reader_state(world& world) noexcept : _world{&world} {}

void reader_state::read_atlases(reader_t& s)
{
    const auto N = s.read<atlasid>();
    atlases.reserve(N * 2);
    for (atlasid i = 0; i < N; i++)
    {
        Vector2ub size;
        s >> size[0];
        s >> size[1];
        const auto& [buf, len] = s.read_asciiz_string<atlas_name_max>();
        atlases[i] = loader.tile_atlas({buf, len}, size);
    }
}

std::shared_ptr<tile_atlas> reader_state::lookup_atlas(atlasid id)
{
    if (auto it = atlases.find(id); it != atlases.end())
        return it->second;
    else
        fm_abort("not such atlas: '%zu'", (std::size_t)id);
}

void reader_state::read_chunks(reader_t& s)
{
    const auto N = s.read<chunksiz>();

    for (std::size_t i = 0; i < N; i++)
    {
        std::decay_t<decltype(chunk_magic)> magic;
        s >> magic;
        if (magic != chunk_magic)
            fm_abort("bad chunk magic");
        chunk_coords coord;
        s >> coord.x;
        s >> coord.y;
        auto& chunk = (*_world)[coord];
        for (std::size_t i = 0; i < TILE_COUNT; i++)
        {
            const tilemeta flags = s.read<tilemeta>();
            tile_ref t = chunk[i];
            using uchar = std::uint8_t;
            const auto make_atlas = [&]() -> tile_image_proto {
                auto id = flags & meta_short_atlasid ? (atlasid)(s.read<uchar>()) : s.read<atlasid>();
                auto v  = flags & meta_short_variant ? (varid)  (s.read<uchar>()) : s.read<varid>();
                auto atlas = lookup_atlas(id);
                fm_assert(v < atlas->num_tiles());
                return { atlas, v };
            };

            if (flags & meta_ground)
                t.ground() = make_atlas();
            if (flags & meta_wall_n)
                t.wall_north() = make_atlas();
            if (flags & meta_wall_w)
                t.wall_west() = make_atlas();

            switch (auto x = pass_mode(flags & pass_mask))
            {
            case pass_shoot_through:
            case pass_blocked:
            case pass_ok:
                t.pass_mode() = x;
                break;
            default:
                fm_abort("bad pass mode '%zu' for tile %zu", i, (std::size_t)x);
            }
        }
    }
}

void reader_state::deserialize_world(ArrayView<const char> buf)
{
    auto s = binary_reader{buf};
    if (!!::memcmp(s.read<std::size(file_magic)-1>().data(), file_magic, std::size(file_magic)-1))
        fm_abort("bad magic");
    std::decay_t<decltype(proto_version)> proto;
    s >> proto;
    if (proto != proto_version)
        fm_abort("bad proto version '%zu' (should be '%zu')",
                 (std::size_t)proto, (std::size_t)proto_version);
    read_atlases(s);
    read_chunks(s);
    s.assert_end();
}

} // namespace floormat::Serialize

namespace floormat {

world world::deserialize(StringView filename)
{
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
    FILE_raii f = ::fopen(filename.data(), "rb");
    if (!f)
    {
        get_error_string(errbuf);
        fm_abort("fopen(\"%s\", \"r\"): %s", filename.data(), errbuf);
    }
    if (int ret = ::fseek(f, 0, SEEK_END); ret != 0)
    {
        get_error_string(errbuf);
        fm_abort("fseek(SEEK_END): %s", errbuf);
    }
    std::size_t len;
    if (auto len_ = ::ftell(f); len_ >= 0)
        len = (std::size_t)len_;
    else
    {
        get_error_string(errbuf);
        fm_abort("ftell: %s", errbuf);
    }
    if (int ret = ::fseek(f, 0, SEEK_SET); ret != 0)
    {
        get_error_string(errbuf);
        fm_abort("fseek(SEEK_SET): %s", errbuf);
    }
    auto buf_ = std::make_unique<char[]>(len);

    if (auto ret = ::fread(&buf_[0], 1, len, f); ret != len)
    {
        get_error_string(errbuf);
        fm_abort("fread short read: %s", errbuf);
    }

    world w;
    Serialize::reader_state s{w};
    s.deserialize_world({buf_.get(), len});
    return w;
}

} // namespace floormat
