#define FM_SERIALIZE_WORLD_IMPL
#include "world-impl.hpp"
#include "binary-reader.inl"
#include "src/world.hpp"
#include "src/loader.hpp"
#include "src/tile-atlas.hpp"

namespace floormat::Serialize {

namespace {

struct reader_state final {
    explicit reader_state(world& world) noexcept;
    void deserialize_world(ArrayView<const char> buf);

private:
    using reader_t = binary_reader<decltype(ArrayView<const char>{}.cbegin())>;

    std::shared_ptr<tile_atlas> lookup_atlas(atlasid id);
    void read_atlases(reader_t& reader);
    void read_chunks(reader_t& reader);

    std::unordered_map<atlasid, std::shared_ptr<tile_atlas>> atlases;
    struct world* world;
};

reader_state::reader_state(struct world& world) noexcept : world{&world} {}

void reader_state::read_atlases(reader_t& s)
{
    const auto N = s.read<atlasid>();
    atlases.reserve(N * 2);
    for (atlasid i = 0; i < N; i++)
    {
        Vector2ub size;
        s >> size[0];
        s >> size[1];
        atlases[i] = loader.tile_atlas(s.read_asciiz_string(), size);
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
        auto& chunk = (*world)[coord];
        for (std::size_t i = 0; i < TILE_COUNT; i++)
        {
            const tilemeta flags = s.read<tilemeta>();
            const auto make_atlas = [&] -> tile_image {
                auto atlas = lookup_atlas(s.read<atlasid>());
                auto id = s.read<imgvar>();
                return { atlas, id };
            };
            tile t;
            if (flags & meta_ground)
                t.ground_image = make_atlas();
            if (flags & meta_wall_n)
                t.wall_north = make_atlas();
            if (flags & meta_wall_w)
                t.wall_west = make_atlas();
            switch (auto x = flags & pass_mask)
            {
            case tile::pass_shoot_through:
            case tile::pass_blocked:
            case tile::pass_ok:
                t.passability = (tile::pass_mode)x;
                break;
            default:
                fm_abort("bad pass mode '%zu' for tile %zu", i, (std::size_t)x);
            }
            chunk[i] = std::move(t);
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

} // namespace

} // namespace floormat::Serialize

namespace floormat {

world world::deserialize(StringView filename)
{
    char errbuf[128];
    constexpr auto strerror = []<std::size_t N> (char (&buf)[N]) -> const char* {
        ::strerror_s(buf, std::size(buf), errno);
        return buf;
    };
    fm_assert(filename.flags() & StringViewFlag::NullTerminated);
    FILE_raii f = ::fopen(filename.data(), "r");
    if (!f)
        fm_abort("fopen(\"%s\", \"r\"): %s", filename.data(), strerror(errbuf));
    if (int ret = ::fseek(f, 0, SEEK_END); ret != 0)
        fm_abort("fseek(SEEK_END): %s", strerror(errbuf));
    std::size_t len;
    if (auto len_ = ::ftell(f); len_ >= 0)
        len = (std::size_t)len_;
    else
        fm_abort("ftell: %s", strerror(errbuf));
    if (int ret = ::fseek(f, 0, SEEK_SET); ret != 0)
        fm_abort("fseek(SEEK_SET): %s", strerror(errbuf));
    auto buf_ = std::make_unique<char[]>(len);
    if (auto ret = ::fread(&buf_[0], len, 1, f); ret != 1)
        fm_abort("fread %zu: %s", len, strerror(errbuf));

    world w;
    Serialize::reader_state s{w};
    s.deserialize_world({buf_.get(), len});
    return w;
}

} // namespace floormat
