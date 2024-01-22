#include "binary-writer.inl"
#include "binary-reader.inl"
#include "compat/defs.hpp"
#include "compat/strerror.hpp"
#include "compat/int-hash.hpp"
#include "compat/exception.hpp"

#include "src/world.hpp"
#include "loader/loader.hpp"

#include "atlas-type.hpp"
#include "src/anim-atlas.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"

#include "src/scenery.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"

#include <cstring>
#include <cstdio>
#include <compare>
#include <memory>
#include <vector>
#include <algorithm>
#include <Corrade/Utility/Path.h>
#include <tsl/robin_map.h>

#if 1
#ifdef __CLION_IDE__
#undef fm_assert
#define fm_assert(...) (void)(__VA_ARGS__)
#endif
#endif

namespace floormat::Serialize {

namespace {

struct FILE_raii final
{
    FILE_raii(FILE* s) noexcept : s{s} {}
    ~FILE_raii() noexcept { close(); }
    operator FILE*() noexcept { return s; }
    void close() noexcept { if (s) std::fclose(s); s = nullptr; }
private:
    FILE* s;
};

using floormat::Hash::fnvhash_buf;

struct string_hasher
{
    inline size_t operator()(StringView s) const
    {
        return fnvhash_buf(s.data(), s.size());
    }
};

template<typename T> T& non_const(const T& value) { return const_cast<T&>(value); }
template<typename T> T& non_const(T& value) = delete;
template<typename T> T& non_const(T&& value) = delete;
template<typename T> T& non_const(const T&& value) = delete;

struct buffer
{
    std::unique_ptr<char[]> data;
    size_t size;

    operator ArrayView<const char>() const { return {&data[0], size}; }
    bool empty() const { return size == 0; }
    buffer() : data{nullptr}, size{0} {}
    buffer(size_t len) : // todo use allocator
        data{std::make_unique<char[]>(len)},
        size{len}
    {
#if !fm_ASAN
        std::memset(&data[0], 0xfe, size);
#endif
    }
};

struct serialized_atlas
{
    buffer buf;
    const void* atlas;
    atlas_type type;
};

struct serialized_chunk
{
    buffer buf{};
    chunk* c;
};

template<typename Derived>
struct visitor_
{
    using tilemeta = uint8_t;
    using atlasid  = uint32_t;
    using chunksiz = uint32_t;
    using proto_t  = uint16_t;

    template<std::unsigned_integral T> static constexpr T highbit = (T{1} << sizeof(T)*8-1);
    template<std::unsigned_integral T> static constexpr T null = T(~highbit<T>);

    static constexpr inline size_t string_max          = 512;
    static constexpr inline proto_t proto_version      = 20;
    static constexpr inline proto_t proto_version_min  = 20;
    static constexpr inline auto file_magic            = ".floormat.save"_s;
    static constexpr inline auto chunk_magic           = maybe_byteswap((uint16_t)0xadde);
    static constexpr inline auto object_magic          = maybe_byteswap((uint16_t)0x0bb0);

    template<typename T, typename F>
    CORRADE_ALWAYS_INLINE void do_visit_nonconst(const T& value, F&& fun)
    {
        do_visit(non_const(value), fun);
    }

    template<typename T, typename F>
    CORRADE_ALWAYS_INLINE void do_visit(T&& value, F&& fun)
    {
        static_cast<Derived&>(*this).visit(value, fun);
    }

    template<typename T, typename F>
    requires (std::is_arithmetic_v<T> && std::is_fundamental_v<T>)
    static void visit(T& x, F&& f)
    {
        f(x);
    }

    template<typename T, size_t N, typename F>
    void visit(Math::Vector<N, T>& x, F&& f)
    {
        for (uint32_t i = 0; i < N; i++)
            do_visit(x.data()[i], f);
    }

    template<typename E, typename F>
    requires std::is_enum_v<E>
    void visit(E& x, F&& f)
    {
        auto* ptr = const_cast<std::underlying_type_t<E>*>(reinterpret_cast<const std::underlying_type_t<E>*>(&x));
        do_visit(*ptr, f);
    }

    template<typename F>
    void visit(object& obj, F&& f)
    {
        auto type = obj.type();

        do_visit(obj.id, f);
        do_visit(type, f);
        fm_assert(obj.atlas);
        do_visit(*obj.atlas, f);
        //do_visit(*obj.c, f);
        do_visit(obj.coord.local(), f);
        do_visit_nonconst(obj.offset, f);
        do_visit_nonconst(obj.bbox_offset, f);
        do_visit_nonconst(obj.bbox_size, f);
        do_visit_nonconst(obj.delta, f);
        do_visit_nonconst(obj.frame, f);
        do_visit_nonconst(obj.r, f);
        do_visit_nonconst(obj.pass, f);

        switch (type)
        {
        case object_type::critter: do_visit(static_cast<critter&>(obj), f); return;
        case object_type::scenery: do_visit(static_cast<scenery&>(obj), f); return;
        case object_type::light:   do_visit(static_cast<light&>(obj), f); return;
        //case object_type::door:   do_visit(static_cast<door&>(obj), f); return;
        case object_type::COUNT:
        case object_type::none:
            break;
        }
        fm_abort("invalid object type '%d'", (int)obj.type());
    }

    template<typename F>
    void visit(tile_ref c, F&& f)
    {
        do_visit(c.ground(), f);
        do_visit(c.wall_north(), f);
        do_visit(c.wall_west(), f);
    }
};

constexpr size_t vector_initial_size = 128, hash_initial_size = vector_initial_size*2;

struct writer final : visitor_<writer>
{
    const world& w;

    std::vector<StringView> string_array{};
    tsl::robin_map<StringView, uint32_t, string_hasher> string_map{hash_initial_size};

    std::vector<serialized_atlas> atlas_array{};
    tsl::robin_map<const void*, uint32_t> atlas_map{hash_initial_size};

    std::vector<serialized_chunk> chunk_array{};

    buffer header_buf{}, string_buf{};

    writer(const world& w) : w{w} {} // avoid spurious warning until GCC 14: warning: missing initializer for member ::<anonymous>

    struct size_counter
    {
        size_t& size;

        template<typename T>
        requires (std::is_arithmetic_v<T> && std::is_fundamental_v<T>)
        void operator()(T) { size += sizeof(T); }
    };

    struct byte_writer
    {
        binary_writer<char*>& s;

        template<typename T>
        requires (std::is_fundamental_v<T> && std::is_arithmetic_v<T>)
        void operator()(T value)
        {
            s << value;
        }
    };

    using visitor_<writer>::visit;

    template<typename F>
    void visit(critter& obj, F&& f)
    {
        uint8_t flags = 0;
        flags |= (1 << 0) * obj.playable;
        do_visit(flags, f);
        do_visit(obj.name, f);
        do_visit(obj.offset_frac, f);
    }

    template<typename F>
    void visit(scenery& obj, F&& f)
    {
        auto sc_type = obj.sc_type;
        do_visit(sc_type, f);
        uint8_t flags = 0;
        flags |= obj.active      * (1 << 0);
        flags |= obj.closing     * (1 << 1);
        flags |= obj.interactive * (1 << 2);
        do_visit(flags, f);
    }

    template<typename F>
    void visit(light& obj, F&& f)
    {
        do_visit(obj.max_distance, f);
        do_visit(obj.color, f);
        auto falloff = obj.falloff;
        do_visit(falloff, f);
        uint8_t flags = 0;
        flags |= obj.enabled * (1 << 0);
        do_visit(flags, f);
    }

    template<typename F>
    void visit(anim_atlas& a, F&& f)
    {
        atlasid id = intern_atlas(&a, atlas_type::object);
        do_visit(id, f);
    }

    template<typename F> void visit(const chunk_coords_& coord, F&& f)
    {
        f(coord.x);
        f(coord.y);
        f(coord.z);
    }

    template<typename F> void visit(const local_coords& pt, F&& f)
    {
        f(pt.to_index());
    }

    template<typename F> void visit(StringView name, F&& f)
    {
        f(intern_string(name));
    }

    template<typename F>
    void intern_atlas_(const void* atlas, atlas_type type, F&& f)
    {
        do_visit(type, f);

        StringView name;

        switch (type)
        {
        case atlas_type::ground: name = reinterpret_cast<const ground_atlas*>(atlas)->name(); goto ok;
        case atlas_type::wall:   name = reinterpret_cast<const wall_atlas*>(atlas)->name(); goto ok;
        case atlas_type::object: name = reinterpret_cast<const anim_atlas*>(atlas)->name(); goto ok;
        case atlas_type::none: break;
        }
        fm_abort("invalid atlas type '%d'", (int)type);

ok:
        do_visit(intern_string(name), f);
    }

    [[nodiscard]] atlasid intern_atlas(const void* atlas, atlas_type type)
    {
        atlas_array.reserve(vector_initial_size);
        fm_assert(atlas != nullptr);
        auto [kv, fresh] = atlas_map.try_emplace(atlas, (uint32_t)-1);
        if (!fresh)
        {
            fm_debug_assert(kv.value() != (uint32_t)-1);
            return kv->second;
        }
        else
        {
            size_t len = 0;
            intern_atlas_(atlas, type, size_counter{len});
            fm_assert(len > 0);

            buffer buf{len};
            binary_writer<char*> s{&buf.data[0], buf.size};
            intern_atlas_(atlas, type, byte_writer{s});
            auto id = (uint32_t)atlas_array.size();
            fm_assert(s.bytes_written() == s.bytes_allocated());
            atlas_array.emplace_back(std::move(buf), atlas, type);
            kv.value() = id;
            fm_assert(id < null<atlasid>);
            return atlasid{id};
        }
    }

    atlasid maybe_intern_atlas(const void* atlas, atlas_type type)
    {
        if (!atlas)
            return null<atlasid>;
        else
            return intern_atlas(atlas, type);
    }

    atlasid intern_string(StringView str)
    {
        string_array.reserve(vector_initial_size);
        fm_assert(!str.find('\0'));
        auto [pair, fresh] = string_map.try_emplace(str, (uint32_t)string_array.size());
        if (fresh)
            string_array.emplace_back(str);
        return pair->second;
    }

    template<typename F>
    void serialize_objects_(chunk& c, F&& f)
    {
        f((uint32_t)c.objects().size());

        for (const std::shared_ptr<object>& obj : c.objects())
        {
            fm_assert(obj != nullptr);
            do_visit(object_magic, f); // todo remove this
            do_visit(*obj, f);
        }
    }

    void serialize_tile_(auto&& g, uint32_t& i, auto&& f)
    {
        using INT = std::decay_t<decltype(g(i))>;
        static_assert(std::is_unsigned_v<INT>);
        constexpr auto highbit = writer::highbit<INT>;
        constexpr auto null = writer::null<INT>;
        const auto a = INT{ g(i) };
        fm_assert(a == null || a < null);
        uint_fast16_t num_idempotent = 0;

        for (uint32_t j = i+1; j < TILE_COUNT; j++)
            if (a != g(j))
                break;
            else if ((size_t)num_idempotent + 1uz == (size_t)null)
                break;
            else
                num_idempotent++;

        if (num_idempotent)
        {
            INT num = highbit | INT(num_idempotent);
            do_visit(num, f);
        }

        if (a != null)
            do_visit(a, f);
        else
            do_visit(null, f);

        i += num_idempotent;
        fm_debug_assert(i <= TILE_COUNT);
    }

    void serialize_chunk_(chunk& c, buffer& buf)
    {
        const auto fn = [this](chunk& c, auto&& f)
        {
            static_assert(null<uint8_t> == 127 && highbit<uint8_t> == 128);
            static_assert(null<uint32_t> == 0x7fffffff && highbit<uint32_t> == 0x80000000);
            do_visit(chunk_magic, f);
            do_visit(c.coord(), f);
            // todo do atlases and variants separately
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&](uint32_t i) {
                    return maybe_intern_atlas(c[i].ground_atlas().get(), atlas_type::ground);
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&](uint32_t i) {
                    return maybe_intern_atlas(c[i].wall_north_atlas().get(), atlas_type::wall);
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&](uint32_t i) {
                    return maybe_intern_atlas(c[i].wall_west_atlas().get(), atlas_type::wall);
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&](uint32_t i) {
                    auto v = c[i].ground().variant; return v == (variant_t)-1 ? null<variant_t> : v;
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&c](uint32_t i) {
                    auto v = c[i].wall_north().variant; return v == (variant_t)-1 ? null<variant_t> : v;
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&c](uint32_t i) {
                    auto v = c[i].wall_west().variant; return v == (variant_t)-1 ? null<variant_t> : v;
                }, i, f);

            serialize_objects_(c, f);
        };

        size_t len = 0;
        auto ctr = size_counter{len};
        fn(c, ctr);
        fm_assert(len > 0);

        buf = buffer{len};
        binary_writer<char*> s{&buf.data[0], buf.size};
        byte_writer b{s};
        fn(c, b);
        fm_assert(s.bytes_written() == s.bytes_allocated());
    }

    template<typename F> void serialize_header_(F&& f)
    {
        fm_assert(header_buf.empty());
        for (char c : file_magic)
            f(c);
        f(proto_version);
        auto nstrings = (uint32_t)string_array.size(),
             natlases = (uint32_t)atlas_array.size(),
             nchunks  = (uint32_t)chunk_array.size();
        do_visit(nstrings, f);
        do_visit(natlases, f);
        do_visit(nchunks, f);
    }

    void serialize_strings_()
    {
        fm_assert(string_buf.empty());
        size_t len = 0;
        for (const auto& s : string_array)
            len += s.size() + 1;
        buffer buf{len};
        binary_writer b{&buf.data[0], buf.size};
        for (const auto& s : string_array)
        {
            fm_assert(s.size() < string_max);
            b.write_asciiz_string(s);
        }
        fm_assert(b.bytes_written() == b.bytes_allocated());
        string_buf = std::move(buf);
    }

    void serialize_world()
    {
        fm_assert(string_array.empty());
        fm_assert(header_buf.empty());
        fm_assert(string_buf.empty());
        fm_assert(atlas_array.empty());
        fm_assert(chunk_array.empty());

        for (auto& [coord, c] : non_const(w.chunks()))
            chunk_array.push_back({.c = &c });

        std::sort(chunk_array.begin(), chunk_array.end(), [](const auto& c1, const auto& c2) {
            auto a = c1.c->coord(), b = c2.c->coord();
            return std::tuple{a.z, a.y, a.x} <=> std::tuple{b.z, b.y, b.x} == std::strong_ordering::less;
        });

        for (uint32_t i = 0; auto& [coord, c] : chunk_array)
            serialize_chunk_(*c, chunk_array[i++].buf);

        {
            size_t len = 0;
            serialize_header_(size_counter{len});
            fm_assert(len > 0);

            buffer hdr{len};
            binary_writer s{&hdr.data[0], hdr.size};
            serialize_header_(byte_writer{s});
            fm_assert(s.bytes_written() == s.bytes_allocated());
            header_buf = std::move(hdr);
        }

        serialize_strings_();
    }
};

struct reader final : visitor_<reader>
{
    std::vector<StringView> strings;
    proto_t PROTO = (proto_t)-1;
    uint32_t nstrings = 0, natlases = 0, nchunks = 0;

    class world& w;
    reader(class world& w) : w{w} {}

    void deserialize_header_(binary_reader<const char*>& s)
    {
        fm_assert(PROTO == (proto_t)-1);
        auto magic = s.read<file_magic.size()>();
        fm_soft_assert(StringView{magic.data(), magic.size()} == file_magic);
        PROTO << s;
        fm_soft_assert(PROTO >= proto_version_min);
        fm_soft_assert(PROTO <= proto_version);
        nstrings << s;
        natlases << s;
        nchunks << s;
    }

    StringView get_string(atlasid id)
    {
        fm_soft_assert(id < strings.size());
        return strings[id];
    }

    void deserialize_strings_(binary_reader<const char*>& s)
    {
        fm_assert(strings.empty());
        strings.reserve(nstrings);
        for (uint32_t i = 0; i < nstrings; i++)
        {
            auto str = s.read_asciiz_string_();
            strings.emplace_back(str);
        }
    }

    void deserialize_world(ArrayView<const char> buf)
    {
        binary_reader s{buf.data(), buf.data() + buf.size()};
        deserialize_header_(s);
        deserialize_strings_(s);
        // atlases
        // chunks
        // assert end
    }
};

void my_fwrite(FILE_raii& f, const buffer& buf, char(&errbuf)[128])
{
    auto len = std::fwrite(&buf.data[0], buf.size, 1, f);
    int error = errno;
    if (len != 1)
        fm_abort("fwrite: %s", get_error_string(errbuf, error).data());
}

} // namespace

} // namespace floormat::Serialize

namespace floormat {

using namespace floormat::Serialize;

void world::serialize(StringView filename)
{
    collect(true);
    char errbuf[128];
    fm_assert(filename.flags() & StringViewFlag::NullTerminated);
    if (Path::exists(filename))
        Path::remove(filename);
    FILE_raii file = std::fopen(filename.data(), "wb");
    if (!file)
    {
        int error = errno;
        fm_abort("fopen(\"%s\", \"w\"): %s", filename.data(), get_error_string(errbuf, error).data());
    }
    {
        struct writer writer{*this};
        const bool is_empty = chunks().empty();
        writer.serialize_world();
        fm_assert(!writer.header_buf.empty());
        if (!is_empty)
        {
            fm_assert(!writer.string_buf.empty());
            fm_assert(!writer.string_array.empty());
            fm_assert(!writer.string_map.empty());
            fm_assert(!writer.atlas_array.empty());
            fm_assert(!writer.atlas_map.empty());
            fm_assert(!writer.chunk_array.empty());
        }
        my_fwrite(file, writer.header_buf, errbuf);
        my_fwrite(file, writer.string_buf, errbuf);
        for (const auto& x : writer.atlas_array)
        {
            fm_assert(!x.buf.empty());
            my_fwrite(file, x.buf, errbuf);
        }
        for (const auto& x : writer.chunk_array)
        {
            fm_assert(!x.buf.empty());
            my_fwrite(file, x.buf, errbuf);
        }
    }

    if (int ret = std::fflush(file); ret != 0)
    {
        int error = errno;
        fm_abort("fflush: %s", get_error_string(errbuf, error).data());
    }
}

class world world::deserialize(StringView filename) noexcept(false)
{
    char errbuf[128];
    buffer buf;

    fm_soft_assert(filename.flags() & StringViewFlag::NullTerminated);
    FILE_raii f = std::fopen(filename.data(), "rb");
    {
        if (!f)
            fm_throw("fopen(\"{}\", \"r\"): {}"_cf, filename, get_error_string(errbuf));
        if (int ret = std::fseek(f, 0, SEEK_END); ret != 0)
            fm_throw("fseek(SEEK_END): {}"_cf, get_error_string(errbuf));
        size_t len;
        if (auto len_ = std::ftell(f); len_ >= 0)
            len = (size_t)len_;
        else
            fm_throw("ftell: {}"_cf, get_error_string(errbuf));
        if (int ret = std::fseek(f, 0, SEEK_SET); ret != 0)
            fm_throw("fseek(SEEK_SET): {}"_cf, get_error_string(errbuf));
        auto buf_ = std::make_unique<char[]>(len+1);
        if (auto ret = std::fread(&buf_[0], 1, len+1, f); ret != len)
            fm_throw("fread short read: {}"_cf, get_error_string(errbuf));

        buf.data = std::move(buf_);
        buf.size = len;
    }

    class world w;
    struct reader r{w};
    r.deserialize_world(buf);

    fm_assert("todo" && false);
    return w;
}

} // namespace floormat
