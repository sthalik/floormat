#include "binary-writer.inl"
#include "binary-reader.inl"
#include "compat/defs.hpp"
#include "compat/debug.hpp"
#include "compat/strerror.hpp"
#include "compat/int-hash.hpp"
#include "compat/exception.hpp"

#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "src/anim-atlas.hpp"
#include "src/scenery.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/world.hpp"

#include "loader/loader.hpp"
#include "loader/vobj-cell.hpp"
#include "atlas-type.hpp"

#include <cstring>
#include <cstdio>
#include <compare>
#include <memory>
#include <vector>
#include <algorithm>
#include <Corrade/Utility/Path.h>
#include <Magnum/Math/Functions.h>
#include <tsl/robin_map.h>

#ifdef __CLION_IDE__
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "cppcoreguidelines-missing-std-forward"
#undef fm_assert
#define fm_assert(...) void(bool((__VA_ARGS__)))
#endif

namespace floormat {

using floormat::Serialize::binary_reader;
using floormat::Serialize::binary_writer;
using floormat::Serialize::atlas_type;
using floormat::Serialize::maybe_byteswap;
using floormat::Hash::fnvhash_buf;

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
    static constexpr inline proto_t proto_version_min  = 20;
    static constexpr inline auto file_magic            = ".floormat.save"_s;
    static constexpr inline auto chunk_magic           = maybe_byteswap((uint16_t)0xadde);
    static constexpr inline auto object_magic          = maybe_byteswap((uint16_t)0x0bb0);

    // ---------- proto versions ----------

    // 19: see old-savegame.cpp
    // 20: complete rewrite
    // 21: oops, forgot the object counter

    static constexpr inline proto_t proto_version      = 21;

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
    void visit_object_internal(object& obj, F&& f, object_id id, object_type type, chunk_coords_ ch)
    {
        non_const(obj.id) = id;
        switch (type)
        {
        case object_type::light:
            static_cast<Derived&>(*this).visit(non_const(obj.atlas), atlas_type::vobj, f);
            break;
        case object_type::scenery:
        case object_type::critter:
            static_cast<Derived&>(*this).visit(non_const(obj.atlas), atlas_type::anim, f);
            break;
        case object_type::none:
        case object_type::COUNT:
            break;
        }
        if (!obj.atlas)
            fm_throw("invalid object type {}"_cf, (int)type);
        //do_visit(*obj.c, f);

        auto pt = obj.coord.local();
        do_visit(pt, f);
        non_const(obj.coord) = {ch, pt};
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
        case object_type::COUNT:
        case object_type::none:
            break;
        }
        fm_abort("invalid object type '%d'", (int)type);
    }

    template<typename F>
    void visit(tile_ref c, F&& f)
    {
        do_visit(c.ground(), f);
        do_visit(c.wall_north(), f);
        do_visit(c.wall_west(), f);
    }

    template<typename F> void visit(chunk_coords_& coord, F&& f)
    {
        f(coord.x);
        f(coord.y);
        f(coord.z);
    }

    enum : uint8_t {
        flag_playable = 1 << 0,
    };

    template<typename F>
    void visit(critter& obj, F&& f)
    {
        do_visit(obj.name, f);
        do_visit(obj.offset_frac, f);

        constexpr struct {
            uint8_t bits;
            bool(*getter)(const critter&);
            void(*setter)(critter&, bool);
        } pairs[] = {
            { flag_playable,
              [](const critter& sc) { return !!sc.playable; },
              [](critter& sc, bool value) { sc.playable = value; }
            },
        };

        uint8_t flags = 0;
        for (auto [bits, getter, setter] : pairs)
            flags |= bits * getter(obj);
        do_visit(flags, f);
        for (auto [bits, getter, setter] : pairs)
            setter(obj, flags & bits);
    }

    enum : uint8_t {
        flag_active      = 1 << 0,
        flag_closing     = 1 << 1,
        flag_interactive = 1 << 2,
    };

    template<typename F> void visit(generic_scenery& s, F&& f)
    {
        constexpr struct {
            uint8_t bits;
            bool(*getter)(const generic_scenery&);
            void(*setter)(generic_scenery&, bool);
        } pairs[] = {
            { flag_active,
                [](const auto& sc) { return !!sc.active; },
                [](auto& sc, bool value) { sc.active = value; }
            },
            { flag_interactive,
                [](const auto& sc) { return !!sc.interactive; },
                [](auto& sc, bool value) { sc.interactive = value; }
            },
        };

        uint8_t flags = 0;
        for (auto [bits, getter, setter] : pairs)
            flags |= bits * getter(s);
        do_visit(flags, f);
        for (auto [bits, getter, setter] : pairs)
            setter(s, flags & bits);
    }

    template<typename F> void visit(door_scenery& s, F&& f)
    {
        constexpr struct {
            uint8_t bits;
            bool(*getter)(const door_scenery&);
            void(*setter)(door_scenery&, bool);
        } pairs[] = {
            { flag_active,
              [](const auto& sc) { return !!sc.active; },
              [](auto& sc, bool value) { sc.active = value; }
            },
            { flag_closing,
              [](const auto& sc) { return !!sc.closing; },
              [](auto& sc, bool value) { sc.closing = value; }
            },
            { flag_interactive,
              [](const auto& sc) { return !!sc.interactive; },
              [](auto& sc, bool value) { sc.interactive = value; }
            },
        };

        uint8_t flags = 0;
        for (auto [bits, getter, setter] : pairs)
            flags |= bits * getter(s);
        do_visit(flags, f);
        for (auto [bits, getter, setter] : pairs)
            setter(s, flags & bits);
    }

    template<typename F> void visit(scenery& obj, F&& f)
    {
        auto sc_type = obj.scenery_type();
        do_visit(sc_type, f);
        if (sc_type != obj.scenery_type())
            obj.subtype = scenery::subtype_from_scenery_type(obj.id, *obj.c, sc_type);

        std::visit(
            [&]<typename T>(T& x) { return do_visit(x, f); },
            obj.subtype
        );
    }

    template<typename F> void visit(light& obj, F&& f)
    {
        do_visit(obj.max_distance, f);
        do_visit(obj.color, f);
        auto falloff = obj.falloff;
        do_visit(falloff, f);
        obj.falloff = falloff;

        constexpr struct {
            uint8_t bits;
            bool(*getter)(const light&);
            void(*setter)(light&, bool);
        } pairs[] = {
            { 1 << 0,
              [](const light& sc) { return !!sc.enabled; },
              [](light& sc, bool value) { sc.enabled = value; }
            },
        };

        uint8_t flags = 0;
        for (auto [bits, getter, setter] : pairs)
            flags |= bits * getter(obj);
        do_visit(flags, f);
        for (auto [bits, getter, setter] : pairs)
            setter(obj, flags & bits);
    }
};

constexpr size_t vector_initial_size = 128, hash_initial_size = vector_initial_size*2;

struct writer final : visitor_<writer>
{
    const world& w;

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
    void visit(std::shared_ptr<anim_atlas>& a, atlas_type type, F&& f)
    {
        atlasid id = intern_atlas(a, type);
        do_visit(id, f);
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
    void visit(object& obj, F&& f, chunk_coords_ ch)
    {
        auto type = obj.type();

        do_visit_nonconst(obj.id, f);
        do_visit(type, f);

        visit_object_internal(obj, f, obj.id, obj.type(), ch);
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
        case atlas_type::vobj:
        case atlas_type::anim:
            name = reinterpret_cast<const anim_atlas*>(atlas)->name();
            goto ok;
        case atlas_type::none:
            break;
        }
        fm_abort("invalid atlas type '%d'", (int)type);

ok:
        do_visit(intern_string(name), f);
    }

    template<typename T> [[nodiscard]] atlasid intern_atlas(const std::shared_ptr<T>& atlas_, atlas_type type)
    {
        const void* atlas = atlas_.get();
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

    template<typename T> atlasid maybe_intern_atlas(const std::shared_ptr<T>& atlas, atlas_type type)
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
            do_visit(object_magic, f); // todo move before all objects
            visit(*obj, f, c.coord());
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

        do_visit(a, f);

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
                    return maybe_intern_atlas(c[i].ground_atlas(), atlas_type::ground);
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&](uint32_t i) {
                    return maybe_intern_atlas(c[i].wall_north_atlas(), atlas_type::wall);
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&](uint32_t i) {
                    return maybe_intern_atlas(c[i].wall_west_atlas(), atlas_type::wall);
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
        f(w.object_counter());
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

void my_fwrite(FILE_raii& f, const buffer& buf, char(&errbuf)[128])
{
    auto len = std::fwrite(&buf.data[0], buf.size, 1, f);
    int error = errno;
    if (len != 1)
        fm_abort("fwrite: %s", get_error_string(errbuf, error).data());
}

} // namespace

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

namespace {

template<atlas_type Type> struct atlas_from_type;
template<> struct atlas_from_type<atlas_type::ground> { using Type = ground_atlas; };
template<> struct atlas_from_type<atlas_type::wall> { using Type = wall_atlas; };
template<> struct atlas_from_type<atlas_type::anim> { using Type = anim_atlas; };
template<> struct atlas_from_type<atlas_type::vobj> { using Type = anim_atlas; };

struct reader final : visitor_<reader>
{
    struct atlas_pair
    {
        void* atlas;
        atlas_type type;
    };

    std::vector<StringView> strings;
    std::vector<atlas_pair> atlases;
    proto_t PROTO = (proto_t)-1;
    object_id object_counter = world::object_counter_init;
    uint32_t nstrings = 0, natlases = 0, nchunks = 0;

    class world& w;
    loader_policy asset_policy;

    reader(class world& w, loader_policy asset_policy) : w{w}, asset_policy{asset_policy} {}

    using visitor_<reader>::visit;

    struct byte_reader
    {
        binary_reader<const char*>& s;

        template<typename T>
        requires (std::is_fundamental_v<T> && std::is_arithmetic_v<T>)
        void operator()(T& value)
        {
            value << s;
        }
    };

    template<typename F>
    void visit(String& str, F&& f)
    {
        atlasid id;
        f(id);
        str = get_string(id);
    }

    template<typename F> void visit(local_coords& pt, F&& f)
    {
        uint8_t i;
        f(i);
        pt = local_coords{i};
    }

    template<typename F>
    void visit(std::shared_ptr<anim_atlas>& a, atlas_type type, F&& f)
    {
        atlasid id = (atlasid)-1;
        f(id);
        switch (type)
        {
        case atlas_type::anim: a = loader.anim_atlas(get_atlas<atlas_type::anim>(id), {}); return;
        case atlas_type::vobj: a = loader.vobj(get_atlas<atlas_type::vobj>(id)).atlas; return;
        case atlas_type::ground:
        case atlas_type::wall:
        case atlas_type::none:
            break;
        }
        fm_throw("invalid atlas type {}"_cf, (int)type);
    }

    template<typename F>
    void visit(std::shared_ptr<object>& obj, F&& f, chunk_coords_ ch)
    {
        object_id id;
        do_visit(id, f);
        std::underlying_type_t<object_type> type_;
        do_visit(type_, f);
        auto type = object_type(type_);

        switch (type)
        {
        case object_type::none:
        case object_type::COUNT:
            break;
        case object_type::light:
            obj = w.make_unconnected_object<light>(); goto ok;
        case object_type::critter:
            obj = w.make_unconnected_object<critter>(); goto ok;
        case object_type::scenery:
            obj = w.make_unconnected_object<scenery>(); goto ok;
        }
        fm_throw("invalid object type {}"_cf, type_);
ok:
        visit_object_internal(*obj, f, id, object_type(type), ch);

        if (PROTO >= 21) [[likely]]
            fm_soft_assert(object_counter >= id);

        if (PROTO == 20) [[unlikely]]
            object_counter = Math::max(object_counter, id);
    }

    bool deserialize_header_(binary_reader<const char*>& s, ArrayView<const char> buf)
    {
        fm_assert(PROTO == (proto_t)-1);
        auto magic = s.read<file_magic.size()>();
        fm_soft_assert(StringView{magic.data(), magic.size()} == file_magic);
        PROTO << s;
        if (PROTO < proto_version_min && PROTO > 0)
        {
            w.deserialize_old(w, buf.exceptPrefix(s.bytes_read()), PROTO, asset_policy);
            return true;
        }
        else
        {
            fm_soft_assert(PROTO >= proto_version_min);
            fm_soft_assert(PROTO <= proto_version);
            if (PROTO >= 21) [[likely]]
            {
                object_counter << s;
                fm_soft_assert(object_counter >= world::object_counter_init);
            }
            nstrings << s;
            natlases << s;
            nchunks << s;
            return false;
        }
    }

    StringView get_string(atlasid id)
    {
        fm_soft_assert(id < strings.size());
        return strings[id];
    }

    template<atlas_type Type>
    StringView get_atlas(atlasid id)
    {
        fm_soft_assert(id < atlases.size());
        auto a = atlases[id];
        fm_soft_assert(a.type == Type);
        using atlas_type = typename atlas_from_type<Type>::Type;
        const auto* atlas = reinterpret_cast<const atlas_type*>(a.atlas);
        return atlas->name();
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

    void deserialize_atlases(binary_reader<const char*>& s)
    {
        for (uint32_t i = 0; i < natlases; i++)
        {
            atlas_type type = (atlas_type)s.read<std::underlying_type_t<atlas_type>>();
            atlasid id; id << s;
            auto str = get_string(id);
            void* atlas = nullptr;
            switch (type)
            {
            case atlas_type::none: break;
            case atlas_type::ground:
                atlas = loader.ground_atlas(str, loader_policy::warn).get();
                goto ok;
            case atlas_type::wall:
                atlas = loader.wall_atlas(str, loader_policy::warn).get();
                goto ok;
            case atlas_type::anim:
                atlas = loader.anim_atlas(str, {}).get();
                goto ok;
            case atlas_type::vobj:
                atlas = loader.vobj(str).atlas.get();
                goto ok;
            }
            fm_throw("invalid atlas_type {}"_cf, (size_t)type);
ok:
            atlases.push_back({.atlas = atlas, .type = type });
        }
    }

    template<typename INT>
    void deserialize_tile_part(auto&& g, uint32_t& i, byte_reader& r)
    {
        constexpr auto highbit = reader::highbit<INT>;
        constexpr auto null = reader::null<INT>;

        INT num;
        uint32_t num_idempotent = 0;

        do_visit(num, r);

        if (num & highbit)
        {
            num_idempotent = num & ~highbit;
            do_visit(num, r);
        }

        if (num != null)
            for (uint32_t j = 0; j <= num_idempotent; j++)
                g(i+j, num);

        i += num_idempotent;
    }

    void deserialize_objects_(chunk& c, byte_reader& r)
    {
        uint32_t count;
        do_visit(count, r);

        for (uint32_t i = 0; i < count; i++)
        {
            using magic_type = std::decay_t<decltype(object_magic)>;
            magic_type magic;
            r(magic);
            fm_soft_assert(magic == object_magic);

            std::shared_ptr<object> obj;
            visit(obj, r, c.coord());
            non_const(obj->coord) = {c.coord(), obj->coord.local()};
            non_const(obj->c) = &c;
            auto ch = c.coord();
            auto pos = obj->coord.local();
            auto coord = global_coords { ch, pos };
            w.do_make_object(obj, coord, false);
        }

        c.sort_objects();
        fm_assert(count == c.objects().size());
    }

    void deserialize_chunk_(binary_reader<const char*>& s)
    {
        auto r = byte_reader{s};

        using magic_type = std::decay_t<decltype(chunk_magic)>;
        magic_type magic; magic << s;
        fm_soft_assert(magic == chunk_magic);

        chunk_coords_ coord;
        do_visit(coord, r);
        auto& c = w[coord];

        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<atlasid>([&](uint32_t i, uint32_t id) {
                auto name = get_atlas<atlas_type::ground>(id);
                auto a = loader.ground_atlas(name, loader_policy::warn);
                c[i].ground() = { std::move(a), (variant_t)-1 };
            }, i, r);
        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<atlasid>([&](uint32_t i, uint32_t id) {
                auto name = get_atlas<atlas_type::wall>(id);
                auto a = loader.wall_atlas(name, loader_policy::warn);
                c[i].wall_north() = { std::move(a), (variant_t)-1 };
            }, i, r);
        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<atlasid>([&](uint32_t i, uint32_t id) {
                auto name = get_atlas<atlas_type::wall>(id);
                auto a = loader.wall_atlas(name, loader_policy::warn);
                c[i].wall_west() = { std::move(a), (variant_t)-1 };
            }, i, r);
        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<variant_t>([&](uint32_t i, variant_t id) {
                c[i].ground().variant = id;
            }, i, r);
        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<variant_t>([&](uint32_t i, variant_t id) {
                c[i].wall_north().variant = id;
            }, i, r);
        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<variant_t>([&](uint32_t i, variant_t id) {
                c[i].wall_west().variant = id;
            }, i, r);

        deserialize_objects_(c, r);
    }

    void deserialize_world(ArrayView<const char> buf)
    {
        binary_reader s{buf.data(), buf.data() + buf.size()};
        if (deserialize_header_(s, buf))
            return;
        deserialize_strings_(s);
        deserialize_atlases(s);
        for (uint32_t i = 0; i < nchunks; i++)
            deserialize_chunk_(s);
        fm_soft_assert(object_counter);
        w.set_object_counter(object_counter);
        s.assert_end();
    }
};

} // namespace

class world world::deserialize(StringView filename, loader_policy asset_policy) noexcept(false)
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
    struct reader r{w, asset_policy};
    r.deserialize_world(buf);

    //fm_assert("todo" && false);
    return w;
}

} // namespace floormat
