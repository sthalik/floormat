#define FM_SERIALIZE_WORLD_IMPL
#include "world-impl.hpp"
#include "binary-reader.inl"
#include "src/world.hpp"

namespace floormat::Serialize {

namespace {

struct reader_state final {
    explicit reader_state(world& world) noexcept;
    void deserialize_world(ArrayView<const char> buf);

private:
    std::shared_ptr<tile_atlas> lookup_atlas(atlasid id);
    void read_atlases();
    void read_chunks();

    std::unordered_map<atlasid, std::shared_ptr<tile_atlas>> atlases;
    struct world* world;
};

reader_state::reader_state(struct world& world) noexcept : world{&world} {}

void reader_state::deserialize_world(ArrayView<const char> buf)
{
    auto s = binary_reader{buf};
    if (!!::memcmp(s.read<std::size(file_magic)-1>().data(), file_magic, std::size(file_magic)-1))
        fm_abort("bad magic");
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
        fm_abort("fseek(\"%s\", 0, SEEK_END): %s", filename.data(), strerror(errbuf));
    std::size_t len;
    if (auto len_ = ::ftell(f); len_ >= 0)
        len = (std::size_t)len_;
    else
        fm_abort("ftell(\"%s\"): %s", filename.data(), strerror(errbuf));
    if (int ret = ::fseek(f, 0, SEEK_SET); ret != 0)
        fm_abort("fseek(\"%s\", 0, SEEK_SET): %s", filename.data(), strerror(errbuf));
    auto buf_ = std::make_unique<char[]>(len);
    if (auto ret = ::fread(&buf_[0], len, 1, f); ret != 1)
        fm_abort("fread(\"%s\", %zu): %s", filename.data(), len, strerror(errbuf));

    world w;
    Serialize::reader_state s{w};
    s.deserialize_world({buf_.get(), len});
    return w;
}

} // namespace floormat
