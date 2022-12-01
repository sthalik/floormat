#define FM_SERIALIZE_WORLD_IMPL
#include "world-impl.hpp"
#include "binary-reader.inl"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "loader/scenery.hpp"
#include "src/tile-atlas.hpp"
#include "src/anim-atlas.hpp"
#include <cstring>
#include <Corrade/Containers/StringStlHash.h>

namespace {

using namespace floormat;
using namespace floormat::Serialize;

struct reader_state final {
    explicit reader_state(world& world) noexcept;
    void deserialize_world(ArrayView<const char> buf);

private:
    using reader_t = binary_reader<decltype(ArrayView<const char>{}.cbegin())>;

    void load_sceneries();
    std::shared_ptr<tile_atlas> lookup_atlas(atlasid id);
    void read_atlases(reader_t& reader);
    void read_sceneries(reader_t& reader);
    void read_chunks(reader_t& reader);

    std::unordered_map<atlasid, std::shared_ptr<tile_atlas>> atlases;
    std::unordered_map<StringView, const serialized_scenery*> default_sceneries;
    std::vector<scenery_proto> sceneries;
    world* _world;
    std::uint16_t PROTO = (std::uint16_t)-1;
};

reader_state::reader_state(world& world) noexcept : _world{&world} {}

void reader_state::load_sceneries()
{
    for (const serialized_scenery& s : loader.sceneries())
        default_sceneries[s.name] = &s;
}

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

template<typename T>
bool read_scenery_flags(binary_reader<T>& s, scenery& sc)
{
    std::uint8_t flags; s >> flags;
    sc.passable    = !!(flags & 1 << 0);
    sc.blocks_view = !!(flags & 1 << 1);
    sc.active      = !!(flags & 1 << 2);
    sc.closing     = !!(flags & 1 << 3);
    sc.interactive = !!(flags & 1 << 4);
    return flags & 1 << 7;
}

void reader_state::read_sceneries(reader_t& s)
{
    std::uint16_t magic; s >> magic;
    if (magic != scenery_magic)
        fm_abort("bad scenery magic");
    atlasid sz; s >> sz;
    fm_assert(sz < scenery_id_max);
    sceneries.resize(sz);

    std::size_t i = 0;
    while (i < sz)
    {
        std::uint8_t num; s >> num;
        fm_assert(num > 0);
        auto str = s.read_asciiz_string<atlas_name_max>();
        auto it = default_sceneries.find(StringView{str.buf, str.len});
        if (it == default_sceneries.end())
            fm_abort("can't find scenery '%s'", str.buf);
        for (std::size_t n = 0; n < num; n++)
        {
            atlasid id; s >> id;
            fm_assert(id < sz);
            scenery_proto sc = it->second->proto;
            bool short_frame = read_scenery_flags(s, sc.frame);
            fm_debug_assert(sc.atlas != nullptr);
            if (short_frame)
                sc.frame.frame = s.read<std::uint8_t>();
            else
                s >> sc.frame.frame;
            fm_assert(sc.frame.frame < sc.atlas->info().nframes);
            sceneries[id] = sc;
        }
        i += num;
    }
    fm_assert(i == sz);
    for (const scenery_proto& x : sceneries)
        fm_assert(x.atlas != nullptr);
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

    for (std::size_t k = 0; k < N; k++)
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
                auto id = flags & meta_short_atlasid ? atlasid{s.read<uchar>()} : s.read<atlasid>();
                variant_t v;
                if (PROTO >= 2) [[likely]]
                    s >> v;
                else
                    v = flags & meta_short_variant_
                        ? s.read<std::uint8_t>()
                        : std::uint8_t(s.read<std::uint16_t>());
                auto atlas = lookup_atlas(id);
                fm_assert(v < atlas->num_tiles());
                return { atlas, v };
            };

            t.pass_mode() = pass_mode(flags & pass_mask);
            if (flags & meta_ground)
                t.ground() = make_atlas();
            if (flags & meta_wall_n)
                t.wall_north() = make_atlas();
            if (flags & meta_wall_w)
                t.wall_west() = make_atlas();
            if (PROTO >= 3) [[likely]]
                if (flags & meta_scenery)
                {
                    atlasid id; s >> id;
                    const bool exact = id & meta_long_scenery_bit;
                    const auto r = rotation(id >> sizeof(id)*8-1-rotation_BITS & rotation_MASK);
                    id &= ~scenery_id_flag_mask;
                    fm_assert(id < sceneries.size());
                    auto sc = sceneries[id];
                    (void)sc.atlas->group(r);
                    sc.frame.r = r;
                    if (!exact)
                    {
                        if (read_scenery_flags(s, sc.frame))
                            sc.frame.frame = s.read<std::uint8_t>();
                        else
                            s >> sc.frame.frame;
                        if (sc.frame.active)
                            s >> sc.frame.delta;
                    }
                    t.scenery() = sc;
                }

            switch (auto x = pass_mode(flags & pass_mask))
            {
            case pass_shoot_through:
            case pass_blocked:
            case pass_ok:
                t.pass_mode() = x;
                break;
            default: [[unlikely]]
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
    proto_t proto;
    s >> proto;
    if (!(proto >= min_proto_version && proto <= proto_version))
        fm_abort("bad proto version '%zu' (should be between '%zu' and '%zu')",
                 (std::size_t)proto, (std::size_t)min_proto_version, (std::size_t)proto_version);
    PROTO = proto;
    load_sceneries();
    read_atlases(s);
    if (PROTO >= 3)
        read_sceneries(s);
    read_chunks(s);
    s.assert_end();
}

} // namespace

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
    reader_state s{w};
    s.deserialize_world({buf_.get(), len});
    return w;
}

} // namespace floormat
