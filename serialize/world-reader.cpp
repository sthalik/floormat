#define FM_SERIALIZE_WORLD_IMPL
#include "world-impl.hpp"
#include "binary-reader.inl"
#include "src/world.hpp"
#include "loader/loader.hpp"
#include "loader/scenery.hpp"
#include "src/tile-atlas.hpp"
#include "src/anim-atlas.hpp"

#include <cstring>

namespace {

using namespace floormat;
using namespace floormat::Serialize;

struct reader_state final {
    explicit reader_state(world& world) noexcept;
    void deserialize_world(ArrayView<const char> buf);

private:
    using reader_t = binary_reader<decltype(ArrayView<const char>{}.cbegin())>;

    const std::shared_ptr<tile_atlas>& lookup_atlas(atlasid id);
    const scenery_proto& lookup_scenery(atlasid id);
    void read_atlases(reader_t& reader);
    void read_sceneries(reader_t& reader);
    void read_chunks(reader_t& reader);

    std::vector<scenery_proto> sceneries;
    std::vector<std::shared_ptr<tile_atlas>> atlases;
    world* _world;
    std::uint16_t PROTO = (std::uint16_t)-1;
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
        auto atlas = loader.tile_atlas({buf, len});
        fm_soft_assert(size == atlas->num_tiles2());
        atlases.push_back(std::move(atlas));
    }
}

template<typename T>
bool read_scenery_flags(binary_reader<T>& s, scenery& sc)
{
    std::uint8_t flags; flags << s;
    sc.passability = pass_mode(flags & pass_mask);
    sc.active      = !!(flags & 1 << 2);
    sc.closing     = !!(flags & 1 << 3);
    sc.interactive = !!(flags & 1 << 4);
    return flags & 1 << 7;
}

void reader_state::read_sceneries(reader_t& s)
{
    (void)loader.sceneries();

    std::uint16_t magic; magic << s;
    if (magic != scenery_magic)
        fm_throw("bad scenery magic"_cf);
    atlasid sz; sz << s;
    fm_soft_assert(sz < scenery_id_max);
    sceneries.resize(sz);

    std::size_t i = 0;
    while (i < sz)
    {
        std::uint8_t num; num << s;
        fm_soft_assert(num > 0);
        auto str = s.read_asciiz_string<atlas_name_max>();
        const auto sc_ = loader.scenery(str);
        for (std::size_t n = 0; n < num; n++)
        {
            atlasid id; id << s;
            fm_soft_assert(id < sz);
            fm_soft_assert(!sceneries[id]);
            scenery_proto sc = sc_;
            bool short_frame = read_scenery_flags(s, sc.frame);
            fm_debug_assert(sc.atlas != nullptr);
            if (short_frame)
                sc.frame.frame = s.read<std::uint8_t>();
            else
                sc.frame.frame << s;
            fm_soft_assert(sc.frame.frame < sc.atlas->info().nframes);
            sceneries[id] = sc;
        }
        i += num;
    }
    fm_soft_assert(i == sz);
}

const std::shared_ptr<tile_atlas>& reader_state::lookup_atlas(atlasid id)
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

void reader_state::read_chunks(reader_t& s)
{
    const auto N = s.read<chunksiz>();

    for (std::size_t k = 0; k < N; k++)
    {
        std::decay_t<decltype(chunk_magic)> magic;
        magic << s;
        if (magic != chunk_magic)
            fm_throw("bad chunk magic"_cf);
        chunk_coords coord;
        coord.x << s;
        coord.y << s;
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
                    v << s;
                else
                    v = flags & meta_short_variant_
                        ? s.read<std::uint8_t>()
                        : std::uint8_t(s.read<std::uint16_t>());
                auto atlas = lookup_atlas(id);
                fm_soft_assert(v < atlas->num_tiles());
                return { atlas, v };
            };

            //t.passability() = pass_mode(flags & pass_mask);
            if (flags & meta_ground)
                t.ground() = make_atlas();
            if (flags & meta_wall_n)
                t.wall_north() = make_atlas();
            if (flags & meta_wall_w)
                t.wall_west() = make_atlas();
            if (PROTO >= 3) [[likely]]
                if (flags & meta_scenery)
                {
                    atlasid id; id << s;
                    const bool exact = id & meta_long_scenery_bit;
                    const auto r = rotation(id >> sizeof(id)*8-1-rotation_BITS & rotation_MASK);
                    id &= ~scenery_id_flag_mask;
                    auto sc = lookup_scenery(id);
                    (void)sc.atlas->group(r);
                    sc.frame.r = r;
                    if (!exact)
                    {
                        if (read_scenery_flags(s, sc.frame))
                            sc.frame.frame = s.read<std::uint8_t>();
                        else
                            sc.frame.frame << s;
                        if (PROTO >= 5) [[likely]]
                        {
                            sc.frame.offset[0] << s;
                            sc.frame.offset[1] << s;
                        }
                        if (sc.frame.active)
                        {
                            if (PROTO >= 4) [[likely]]
                                sc.frame.delta << s;
                            else
                                sc.frame.delta = (std::uint16_t)Math::clamp(int(s.read<float>() * 65535), 0, 65535);
                        }
                    }
                    t.scenery() = sc;
                }
        }
    }
}

void reader_state::deserialize_world(ArrayView<const char> buf)
{
    auto s = binary_reader{buf};
    if (!!::memcmp(s.read<std::size(file_magic)-1>().data(), file_magic, std::size(file_magic)-1))
        fm_throw("bad magic"_cf);
    proto_t proto;
    proto << s;
    if (!(proto >= min_proto_version && proto <= proto_version))
        fm_throw("bad proto version '{}' (should be between '{}' and '{}')"_cf,
                 (std::size_t)proto, (std::size_t)min_proto_version, (std::size_t)proto_version);
    PROTO = proto;
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
    fm_soft_assert(filename.flags() & StringViewFlag::NullTerminated);
    FILE_raii f = ::fopen(filename.data(), "rb");
    if (!f)
    {
        get_error_string(errbuf);
        fm_throw("fopen(\"{}\", \"r\"): {}"_cf, filename.data(), errbuf);
    }
    if (int ret = ::fseek(f, 0, SEEK_END); ret != 0)
    {
        get_error_string(errbuf);
        fm_throw("fseek(SEEK_END): {}"_cf, errbuf);
    }
    std::size_t len;
    if (auto len_ = ::ftell(f); len_ >= 0)
        len = (std::size_t)len_;
    else
    {
        get_error_string(errbuf);
        fm_throw("ftell: {}"_cf, errbuf);
    }
    if (int ret = ::fseek(f, 0, SEEK_SET); ret != 0)
    {
        get_error_string(errbuf);
        fm_throw("fseek(SEEK_SET): {}"_cf, errbuf);
    }
    auto buf_ = std::make_unique<char[]>(len);

    if (auto ret = ::fread(&buf_[0], 1, len, f); ret != len)
    {
        get_error_string(errbuf);
        fm_throw("fread short read: {}"_cf, errbuf);
    }

    world w;
    reader_state s{w};
    s.deserialize_world({buf_.get(), len});
    return w;
}

} // namespace floormat
