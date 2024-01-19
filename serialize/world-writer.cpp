#include "world-impl.hpp"
#include "binary-writer.inl"
#include "compat/strerror.hpp"
#include "compat/int-hash.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"

#include "atlas-type.hpp"
#include "src/anim-atlas.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"

#include "src/scenery.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"

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
    struct string_container
    {
        StringView str;
        bool operator==(const string_container&) const = default;

        friend void swap(string_container& a, string_container& b)
        {
            auto tmp = a.str;
            a.str = b.str;
            b.str = tmp;
        }
    };
}

} // namespace floormat::Serialize

using floormat::Serialize::string_container;
using floormat::Hash::fnvhash_buf;

template<> struct std::hash<string_container>
{
    size_t operator()(const string_container& x) const noexcept
    {
        return fnvhash_buf(x.str.data(), x.str.size());
    }
};


namespace floormat::Serialize {

namespace {

template<typename T> T& non_const(const T& value) { return const_cast<T&>(value); }
template<typename T> T& non_const(T& value) = delete;
template<typename T> T& non_const(T&& value) = delete;
template<typename T> T& non_const(const T&& value) = delete;

constexpr size_t vector_initial_size = 128, hash_initial_size = vector_initial_size*2;

template<typename T>
struct magic
{
    using type = T;
    uint16_t magic;
};

struct buffer
{
    std::unique_ptr<char[]> data;
    size_t size;

    bool empty() const { return size == 0; }
    buffer() : data{nullptr}, size{0} {}
    buffer(size_t len) : // todo use allocator
        data{std::make_unique<char[]>(len)},
        size{len}
    {
        std::memset(&data[0], 0xfe, size);
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
    const chunk* c;
};

template<typename Derived>
struct visitor_
{
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
    void visit(T& x, F&& f)
    {
        f(x);
    }

    template<typename T, size_t N, typename F>
    void visit(Math::Vector<N, T>& x, F&& f)
    {
        for (uint32_t i = 0; i < N; i++)
            do_visit(x.data()[i], f);
    }

#if 0
    template<typename F>
    void visit(StringView str, F&& f)
    {
        f(str);
    }
#endif

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
        do_visit(obj.id, f);
        do_visit(obj.type, f);
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

        switch (obj.type)
        {
        case object_type::critter:          do_visit(static_cast<critter&>(obj), f); return;
        case object_type::generic_scenery:  do_visit(static_cast<scenery&>(obj), f); return;
        case object_type::light:            do_visit(static_cast<light&>(obj), f); return;
        case object_type::door:             do_visit(static_cast<door&>(obj), f); return;
        case object_type::COUNT:
        case object_type::none:
            break;
        }
        fm_abort("invalid object type '%d'", (int)obj.type);
    }

    template<typename F>
    void visit(tile_ref c, F&& f)
    {
        do_visit(c.ground(), f);
        do_visit(c.wall_north(), f);
        do_visit(c.wall_west(), f);
    }
};

struct writer final : visitor_<writer>
{
    const world& w;

    std::vector<StringView> string_array{};
    tsl::robin_map<string_container, uint32_t> string_map{hash_initial_size};

    std::vector<serialized_atlas> atlas_array{};
    tsl::robin_map<const void*, uint32_t> atlas_map{hash_initial_size};

    std::vector<serialized_chunk> chunk_array{vector_initial_size};

    buffer header_buf{};

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
    void intern_atlas_(void* atlas, atlas_type type, F&& f)
    {
        do_visit(atlas_magic, f);
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

ok:     do_visit(intern_string(name), f);
    }

    [[nodiscard]] atlasid intern_atlas(void* atlas, atlas_type type)
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
            fm_assert(id != null_atlas);
            return atlasid{id};
        }
    }

    atlasid maybe_intern_atlas(void* atlas, atlas_type type)
    {
        if (!atlas)
            return null_atlas;
        else
            return intern_atlas(atlas, type);
    }

    atlasid intern_string(StringView str)
    {
        string_array.reserve(vector_initial_size);
        auto [kv, found] = string_map.try_emplace({str}, (uint32_t)-1);
        if (found)
            return kv.value();
        else
        {
            auto id = (uint32_t)string_array.size();
            string_array.emplace_back(str);
            kv.value() = id;
            fm_assert(id != null_atlas);
            return atlasid{id};
        }
    }

    template<typename F>
    void serialize_objects_(chunk& c, F&& f)
    {
        f((uint32_t)c.objects().size());

        for (const std::shared_ptr<object>& obj : c.objects())
        {
            fm_assert(obj != nullptr);
            do_visit(object_magic, f);
            do_visit(*obj, f);
        }
    }

    template<typename F>
    void serialize_tile_(tile_ref t, F&& f)
    {
        auto g = maybe_intern_atlas(t.ground_atlas().get(), atlas_type::ground),
             n = maybe_intern_atlas(t.wall_north_atlas().get(), atlas_type::wall),
             w = maybe_intern_atlas(t.wall_west_atlas().get(), atlas_type::wall);
        do_visit(g, f);
        do_visit(n, f);
        do_visit(w, f);
        if (g != null_atlas) do_visit(t.ground().variant, f);
        if (n != null_atlas) do_visit(t.wall_north().variant, f);
        if (w != null_atlas) do_visit(t.wall_west().variant, f);
    }

    void serialize_chunk_(chunk& c)
    {
        size_t len = 0;
        {
            auto ctr = size_counter{len};
            do_visit(chunk_magic, ctr);
            do_visit(c.coord(), ctr);
            fm_assert(len > 0);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_(c[i], ctr);
            serialize_objects_(c, ctr);
        }

        buffer buf{len};
        {
            binary_writer<char*> s{&buf.data[0], buf.size};
            byte_writer b{s};
            do_visit(chunk_magic, b);
            do_visit(c.coord(), b);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_(c[i], b);
            serialize_objects_(c, b);
            fm_assert(s.bytes_written() == s.bytes_allocated());
        }
        chunk_array.emplace_back(std::move(buf), &c);
    }

    template<typename F>
    void serialize_header_(F&& f)
    {
        fm_assert(header_buf.empty());
        for (char c : file_magic)
            f(c);
        auto nstrings = (uint32_t)string_array.size(),
             natlases = (uint32_t)atlas_array.size(),
             nchunks = (uint32_t)chunk_array.size();
        do_visit(nstrings, f);
        do_visit(natlases, f);
        do_visit(nchunks, f);
    }

    void serialize_world()
    {
        fm_assert(string_array.empty());
        fm_assert(atlas_array.empty());
        fm_assert(chunk_array.empty());
        fm_assert(header_buf.empty());

        struct pair { chunk_coords_ coord; chunk* c; };
        std::vector<pair> chunks;
        chunks.reserve(w.chunks().size());

        for (auto& [coord, c] : w.chunks())
            chunks.push_back(pair{coord, &non_const(c)});
        std::sort(chunks.begin(), chunks.end(), [](const auto& at, const auto& bt) {
            auto a = at.coord, b = bt.coord;
            return std::tuple{a.z, a.y, a.x} <=> std::tuple{b.z, b.y, b.x} == std::strong_ordering::less;
        });

        for (auto [coord, c] : chunks)
            serialize_chunk_(*c);

        size_t len = 0;
        {
            fm_assert(header_buf.empty());
            serialize_header_(size_counter{len});
            fm_assert(len > 0);
        }
        buffer hdr{len};
        {
            binary_writer<char*> s{&hdr.data[0], hdr.size};
            serialize_header_(byte_writer{s});
            fm_assert(s.bytes_written() == s.bytes_allocated());
        }
        header_buf = std::move(hdr);
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
};

void my_fwrite(FILE_raii& f, const buffer& buf, char(&errbuf)[128])
{
    auto len = ::fwrite(&buf.data[0], buf.size, 1, f);
    int error = errno;
    if (len != 1)
        fm_abort("fwrite: %s", get_error_string(errbuf, error).data());
}

} // namespace

} // namespace floormat::Serialize

namespace floormat {

void world::serialize(StringView filename)
{
    using namespace floormat::Serialize;

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
    {
        struct writer writer{.w = *this};
        const bool is_empty = chunks().empty();
        writer.serialize_world();
        if (!is_empty)
        {
            fm_assert(!writer.header_buf.empty());
            fm_assert(!writer.atlas_array.empty());
            fm_assert(!writer.atlas_map.empty());
            fm_assert(!writer.string_array.empty());
            fm_assert(!writer.string_map.empty());
            fm_assert(!writer.chunk_array.empty());
        }
        my_fwrite(file, writer.header_buf, errbuf);

    }

    if (int ret = ::fflush(file); ret != 0)
    {
        int error = errno;
        fm_abort("fflush: %s", get_error_string(errbuf, error).data());
    }
}

} // namespace floormat
