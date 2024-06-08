#include "binary-writer.inl"
#include "binary-reader.inl"
#include "compat/defs.hpp"
#include "compat/non-const.hpp"
#include "compat/strerror.hpp"
#include "compat/int-hash.hpp"
#include "compat/exception.hpp"

#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "src/anim-atlas.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/world.hpp"

#include "loader/loader.hpp"
#include "loader/vobj-cell.hpp"
#include "atlas-type.hpp"

#include <cstring>
#include <cstdio>
#include <compare>
#include <concepts>
#include <memory>
#include <vector>
#include <algorithm>
#include <Corrade/Utility/Path.h>
//#include <Magnum/Math/Functions.h>
#include <tsl/robin_map.h>

// ReSharper disable CppDFAUnreachableCode
// ReSharper disable CppDFAUnreachableFunctionCall
// zzReSharper disable CppUseStructuredBinding
/*
NOLINTBEGIN(
  *-missing-std-forward, *-avoid-const-or-ref-data-members,
  *-redundant-member-init, *-redundant-inline-specifier,
  *-rvalue-reference-param-not-moved, *-redundant-casting
)
*/

namespace floormat {

using floormat::Serialize::binary_reader;
using floormat::Serialize::binary_writer;
using floormat::Serialize::atlas_type;
using floormat::Serialize::maybe_byteswap;

namespace {

struct FILE_raii final
{
    explicit FILE_raii(FILE* s) noexcept : s{s} {}
    ~FILE_raii() noexcept { close(); }
    operator FILE*() noexcept { return s; }
    void close() noexcept { if (s) std::fclose(s); s = nullptr; }
private:
    FILE* s;
};

struct string_hasher { CORRADE_ALWAYS_INLINE size_t operator()(StringView s) const { return hash_buf(s.data(), s.size()); } };

template<typename T> concept Number = std::is_arithmetic_v<std::remove_cvref_t<T>>;
template<typename T> concept Enum = std::is_enum_v<std::remove_cvref_t<T>>;
template<typename T> concept Vector = Math::IsVector<std::remove_cvref_t<T>>::value;

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

struct size_counter
{
    size_t& size;

    template<typename T> requires (std::is_arithmetic_v<T> && std::is_fundamental_v<T>)
    CORRADE_ALWAYS_INLINE void operator()(T) { size += sizeof(T); }
};

struct byte_writer
{
    binary_writer<char*>& s;

    template<typename T> requires (std::is_fundamental_v<T> && std::is_arithmetic_v<T>)
    CORRADE_ALWAYS_INLINE void operator()(T value) { s << value; }
};

struct byte_reader
{
    binary_reader<const char*>& s;

    template<typename T> requires (std::is_fundamental_v<T> && std::is_arithmetic_v<T>)
    CORRADE_ALWAYS_INLINE void operator()(T& value) { value << s; }
};

struct object_header_s
{
    object_id& id;
    object_type& type;
    chunk* const& ch;
    local_coords& tile;
};

struct critter_header_s
{
    uint16_t& offset_frac;
};

using proto_t  = uint16_t;

template<typename Derived, bool IsWriter>
struct visitor_
{
    explicit visitor_() = default;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(visitor_);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(visitor_);

    // ---------- proto versions ----------
    // 19: see old-savegame.cpp
    // 20: complete rewrite
    // 21: oops, forgot the object counter
    // 22: add object::speed
    // 23: switch object::delta to 32-bit
    // 24: switch object::offset_frac from Vector2us to uint16_t
    static constexpr proto_t proto_version = 24;

    static constexpr size_t string_max          = 512;
    static constexpr proto_t proto_version_min  = 20;
    static constexpr auto file_magic            = ".floormat.save"_s;
    static constexpr auto chunk_magic           = maybe_byteswap((uint16_t)0xadde);
    static constexpr auto object_magic          = maybe_byteswap((uint16_t)0x0bb0);

    using tilemeta = uint8_t;
    using atlasid  = uint32_t;
    using chunksiz = uint32_t;

    template<std::unsigned_integral T> static constexpr T highbit = (T{1} << sizeof(T)*8-1);
    template<std::unsigned_integral T> static constexpr T null = T(~highbit<T>);

    template<typename T, typename U> using qual2 = std::conditional_t<IsWriter, const T, U>;
    template<typename T> using qual = std::conditional_t<IsWriter, const T, T>;

    using o_object  = qual2<object, object_proto>;
    using o_critter = qual2<critter, critter_proto>;
    using o_scenery = qual2<scenery, scenery_proto>;
    using o_light   = qual2<light, light_proto>;
    using o_sc_g    = qual2<generic_scenery, generic_scenery_proto>;
    using o_sc_door = qual2<door_scenery, door_scenery_proto>;

    template<Number T, typename F> requires (!IsWriter) static CORRADE_ALWAYS_INLINE void visit(T& x, F&& f) { f(x); }
    template<Number T, typename F> requires (IsWriter) static CORRADE_ALWAYS_INLINE void visit(T x, F&& f) { f(x); }

    CORRADE_ALWAYS_INLINE Derived& derived() { return static_cast<Derived&>(*this); }

    template<Enum E, typename F> requires (!IsWriter) CORRADE_ALWAYS_INLINE void visit(E& x, F&& f)
    { using U = std::underlying_type_t<std::remove_cvref_t<E>>; auto xʹ = U(0); f(xʹ); x = E(xʹ); }
    template<Enum E, typename F> requires (IsWriter) CORRADE_ALWAYS_INLINE void visit(E x, F&& f)
    { using U = std::underlying_type_t<E>; f(static_cast<U>(x)); }

    template<Vector T, typename F> void visit(T&& x, F&& f)
    {
        constexpr auto N = std::remove_cvref_t<T>::Size;
        for (uint32_t i = 0; i < N; i++)
            visit(forward<T>(x).data()[i], f);
    }

    template<typename F>
    void visit_object_header(o_object& obj, const object_header_s& s, F&& f)
    {
        auto& self = derived();

        visit(s.id, f);
        fm_soft_assert(s.id != 0);

        visit(s.type, f);
        if (s.type >= object_type::COUNT || s.type == object_type::none) [[unlikely]]
            fm_throw("invalid object type {}"_cf, (int)s.type);
        switch (s.type)
        {
        case object_type::none:
        case object_type::COUNT: std::unreachable();
        case object_type::light:
            self.visit(obj.atlas, atlas_type::vobj, f);
            break;
        case object_type::scenery:
        case object_type::critter:
            self.visit(obj.atlas, atlas_type::anim, f);
            break;
        case object_type::hole:
            fm_abort("todo! not implemented");
        }
        fm_debug_assert(obj.atlas);

        self.visit(s.tile, f);
        visit(non_const_(obj.offset), f);
        visit(non_const_(obj.bbox_offset), f);
        visit(non_const_(obj.bbox_size), f);
        if (self.PROTO >= 23) [[likely]]
            visit(obj.delta, f);
        else
        {
            if constexpr(!IsWriter)
            {
                auto delta_ = uint16_t(obj.delta >> 16);
                visit(delta_, f);
                non_const_(obj.delta) = delta_ * 65536u;
            }
            else
            {
                std::unreachable();
                fm_assert(false);
            }
        }
        visit(obj.frame, f);
        visit(obj.r, f);
        visit(obj.pass, f);
    }

    template<typename F> static inline void visit(tile_ref c, F&& f)
    {
        do_visit(c.ground(), f);
        do_visit(c.wall_north(), f);
        do_visit(c.wall_west(), f);
    }

    template<typename F> static inline void visit(qual<chunk_coords_>& coord, F&& f)
    {
        visit(coord.x, f);
        visit(coord.y, f);
        visit(coord.z, f);
    }

    enum : uint8_t {
        flag_playable = 1 << 0,
    };

    enum : uint8_t {
        flag_active      = 1 << 0,
        flag_closing     = 1 << 1,
        flag_interactive = 1 << 2,
    };

    template<typename F> void visit_scenery_proto(o_sc_g& s, F&& f)
    {
        using T = std::conditional_t<IsWriter, generic_scenery, generic_scenery_proto>;
        constexpr struct {
            uint8_t bits;
            bool(*getter)(const T&);
            void(*setter)(T&, bool);
        } pairs[] = {
            { flag_active,
                [](const T& sc) { return !!sc.active; },
                [](T& sc, bool value) { sc.active = value; }
            },
            { flag_interactive,
                [](const T& sc) { return !!sc.interactive; },
                [](T& sc, bool value) { sc.interactive = value; }
            },
        };

        // todo! make function
        if constexpr(IsWriter)
        {
            uint8_t flags = 0;
            for (auto [bits, getter, setter] : pairs)
                flags |= bits * getter(s);
            visit(flags, f);
        }
        else
        {
            uint8_t flags = 0;
            visit(flags, f);
            for (auto [bits, getter, setter] : pairs)
                setter(s, flags & bits);
        }
    }

    template<typename F> void visit_scenery_proto(o_sc_door& s, F&& f)
    {
        using T = std::conditional_t<IsWriter, door_scenery, door_scenery_proto>;
        constexpr struct {
            uint8_t bits;
            bool(*getter)(const T&);
            void(*setter)(T&, bool);
        } pairs[] = {
            { flag_active,
              [](const T& sc) { return !!sc.active; },
              [](T& sc, bool value) { sc.active = value; }
            },
            { flag_closing,
              [](const auto& sc) { return !!sc.closing; },
              [](T& sc, bool value) { sc.closing = value; }
            },
            { flag_interactive,
              [](const T& sc) { return !!sc.interactive; },
              [](T& sc, bool value) { sc.interactive = value; }
            },
        };

        if constexpr(IsWriter)
        {
            uint8_t flags = 0;
            for (auto [bits, getter, setter] : pairs)
                flags |= bits * getter(s);
            visit(flags, f);
        }
        else
        {
            uint8_t flags = 0;
            visit(flags, f);
            for (auto [bits, getter, setter] : pairs)
                setter(s, flags & bits);
        }
    }

    template<typename F> void visit_object_proto(o_critter& obj, critter_header_s&& s, F&& f)
    {
        auto& self = derived();

        self.visit(obj.name, f);

        if (self.PROTO >= 22) [[likely]]
            visit(obj.speed, f);
        fm_soft_assert(obj.speed >= 0);

        if (self.PROTO >= 24) [[likely]]
            visit(s.offset_frac, f);
        else
        {
            Vector2us foo1;
            visit(foo1, f);
            s.offset_frac = 0;
        }
        visit(obj.playable, f);
    }

    template<typename F>
    void visit_object_proto(o_light& s, std::nullptr_t, F&& f)
    {
        visit(s.max_distance, f);
        visit(s.color, f);
        visit(s.falloff, f);
        visit(s.enabled, f);
    }
};

constexpr size_t vector_initial_size = 128, hash_initial_size = vector_initial_size*2;

struct writer final : visitor_<writer, true>
{
    using visitor_<writer, true>::visit;

    struct serialized_atlas
    {
        [[maybe_unused]] buffer buf;
        [[maybe_unused]] const void* atlas;
        [[maybe_unused]] atlas_type type;
    };

    struct serialized_chunk
    {
        buffer buf{};
        chunk* c;
    };

    static constexpr proto_t PROTO = proto_version;

    const world& w;

    std::vector<StringView> string_array{};
    tsl::robin_map<StringView, uint32_t, string_hasher> string_map{hash_initial_size};

    std::vector<serialized_atlas> atlas_array{};
    tsl::robin_map<const void*, uint32_t> atlas_map{hash_initial_size};

    std::vector<serialized_chunk> chunk_array{};

    buffer header_buf{}, string_buf{};

    explicit writer(const world& w) : w{ w } {}

    template<typename F> static void visit(const local_coords& pt, F&& f) { visit(pt.to_index(), f); }
    template<typename F> void visit(StringView name, F&& f) { visit(intern_string(name), f); }

    template<typename F> void visit(qual<std::shared_ptr<anim_atlas>>& a, atlas_type type, F&& f)
    { atlasid id = intern_atlas(a, type); visit(id, f); }

    template<typename F> void write_scenery_proto(const scenery& obj, F&& f)
    {
        auto sc_type = obj.scenery_type();
        fm_debug_assert(sc_type != scenery_type::none && sc_type < scenery_type::COUNT);
        visit(sc_type, f);
        switch (sc_type)
        {
        case scenery_type::generic: visit_scenery_proto(static_cast<const generic_scenery&>(obj), f); break;
        case scenery_type::door:    visit_scenery_proto(static_cast<const door_scenery&>(obj), f); break;
        case scenery_type::none:
        case scenery_type::COUNT:
            std::unreachable();
        }
    }

    template<typename F> void write_object(o_object& obj, chunk* c, F&& f)
    {
        auto id = obj.id;
        auto type = obj.type();
        auto tile = obj.coord.local();
        const object_header_s s{
            .id = id,
            .type = type,
            .ch = c,
            .tile = tile,
        };
        visit_object_header(obj, s, f);
        fm_assert(s.id != 0);
        switch (type)
        {
        case object_type::none:
        case object_type::COUNT:
            break;
        case object_type::critter:
        {
            uint16_t offset_frac = 0;
            critter_header_s cr = {
                .offset_frac = offset_frac,
            };
            visit_object_proto(static_cast<const critter&>(obj), move(cr), f);
            goto ok;
        }
        case object_type::light:
            visit_object_proto(static_cast<const light&>(obj), {}, f);
            goto ok;
        case object_type::scenery:
            write_scenery_proto(static_cast<const scenery&>(obj), f);
            goto ok;
        case object_type::hole:
            fm_abort("todo! not implemented");
        }
        fm_assert(false);
ok:     void();
    }

    template<typename F> void intern_atlas_(const void* atlas, atlas_type type, F&& f)
    {
        visit(type, f);

        StringView name;

        if (type >= atlas_type::COUNT || type == atlas_type::none) [[unlikely]]
            fm_abort("invalid atlas type '%d'", (int)type);
        switch (type)
        {
        case atlas_type::ground:
            name = static_cast<const ground_atlas*>(atlas)->name(); break;
        case atlas_type::wall:
            name = static_cast<const wall_atlas*>(atlas)->name(); break;
        case atlas_type::vobj:
        case atlas_type::anim:
            name = static_cast<const anim_atlas*>(atlas)->name(); break;
        case atlas_type::none:
        case atlas_type::COUNT: std::unreachable();
        }
        visit(intern_string(name), f);
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
            atlas_array.emplace_back(move(buf), atlas, type);
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

    template<typename F> void serialize_objects_(chunk& c, F&& f)
    {
        uint32_t count = 0;
        for (const std::shared_ptr<object>& obj : c.objects())
        {
            if (obj->ephemeral)
                continue;
            count++;
        }
        visit(count, f);

        for (const std::shared_ptr<object>& obj : c.objects())
        {
            fm_assert(obj != nullptr);
            if (obj->ephemeral)
                continue;
            auto magic = object_magic;
            visit(magic, f);
            write_object(*obj, &obj->chunk(), f);
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
            visit(num, f);
        }

        visit(a, f);

        i += num_idempotent;
        fm_debug_assert(i <= TILE_COUNT);
    }

    void serialize_chunk_(chunk& c, buffer& buf)
    {
        const auto fn = [this](chunk& ch, auto&& f)
        {
            static_assert(null<uint8_t> == 127 && highbit<uint8_t> == 128);
            static_assert(null<uint32_t> == 0x7fffffff && highbit<uint32_t> == 0x80000000);
            auto magic = chunk_magic;
            visit(magic, f);
            visit(ch.coord(), f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&](uint32_t index) {
                    return maybe_intern_atlas(ch[index].ground_atlas(), atlas_type::ground);
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&](uint32_t index) {
                    return maybe_intern_atlas(ch[index].wall_north_atlas(), atlas_type::wall);
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&](uint32_t index) {
                    return maybe_intern_atlas(ch[index].wall_west_atlas(), atlas_type::wall);
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&](uint32_t index) {
                    auto v = ch[index].ground().variant; return v == (variant_t)-1 ? null<variant_t> : v;
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&ch](uint32_t index) {
                    auto v = ch[index].wall_north().variant; return v == (variant_t)-1 ? null<variant_t> : v;
                }, i, f);
            for (uint32_t i = 0; i < TILE_COUNT; i++)
                serialize_tile_([&ch](uint32_t index) {
                    auto v = ch[index].wall_west().variant; return v == (variant_t)-1 ? null<variant_t> : v;
                }, i, f);

            serialize_objects_(ch, f);
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
            visit(c, f);
        visit(proto_version, f);
        visit(w.object_counter(), f);
        auto nstrings = (uint32_t)string_array.size(),
             natlases = (uint32_t)atlas_array.size(),
             nchunks  = (uint32_t)chunk_array.size();
        visit(nstrings, f);
        visit(natlases, f);
        visit(nchunks, f);
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
        string_buf = move(buf);
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
            header_buf = move(hdr);
        }

        serialize_strings_();
    }
};

template struct visitor_<writer, true>;

void my_fwrite(FILE_raii& f, const buffer& buf, char(&errbuf)[128])
{
    if (buf.size > 0) [[likely]]
    {
        auto len = std::fwrite(&buf.data[0], buf.size, 1, f);
        int error = errno;
        if (len != 1)
            fm_abort("fwrite: %s", get_error_string(errbuf, error).data());
    }
}

} // namespace

void world::serialize(StringView filename)
{
    collect(true);
    char errbuf[128];
    fm_assert(filename.flags() & StringViewFlag::NullTerminated);
    if (Path::exists(filename))
        Path::remove(filename);
    FILE_raii file{std::fopen(filename.data(), "wb")};
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

struct reader final : visitor_<reader, false>
{
    using visitor_<reader, false>::visit;
    using visitor_<reader, false>::visit_object_proto;

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

    reader(class world& w, loader_policy policy) : w{w}, asset_policy{policy} {}

    template<typename F> void visit(String& str, F&& f)
    { atlasid id; f(id); str = get_string(id); }

    template<typename F> static void visit(local_coords& pt, F&& f)
    { uint8_t i; f(i); pt = local_coords{i}; }

    template<typename F> void visit(std::shared_ptr<anim_atlas>& a, atlas_type type, F&& f)
    {
        atlasid id = (atlasid)-1;
        f(id);
        switch (type)
        {
        case atlas_type::ground:
        case atlas_type::wall:
        case atlas_type::none:
        default:
            fm_throw("invalid atlas type {}"_cf, (int)type);
        case atlas_type::anim:
            a = loader.anim_atlas(get_atlas<atlas_type::anim>(id), {}, loader_policy::warn);
            break;
        case atlas_type::vobj:
            a = loader.vobj(get_atlas<atlas_type::vobj>(id)).atlas;
            break;
        }
    }

    template<typename F>
    void read_scenery(std::shared_ptr<scenery>& ret, const object_proto& pʹ, const object_header_s& h, F&& f)
    {
        const auto coord = global_coords{h.ch->coord(), h.tile};
        auto sc_type = scenery_type::none;
        visit(sc_type, f);

        scenery_proto sc;
        static_cast<object_proto&>(sc) = pʹ;

        switch (sc_type)
        {
        case scenery_type::none:
        case scenery_type::COUNT:
            break;
        case scenery_type::generic: {
            generic_scenery_proto p;
            visit_scenery_proto(p, f);
            sc.subtype = move(p);
            goto ok;
        }
        case scenery_type::door: {
            door_scenery_proto p;
            visit_scenery_proto(p, f);
            sc.subtype = move(p);
            goto ok;
        }
        }
        fm_throw("invalid sc_type {}"_cf, (int)sc_type);
ok:
        ret = w.make_scenery(h.id, coord, move(sc));
    }

    template<typename Obj, typename Proto, typename Header>
    std::shared_ptr<object> make_object(const object_header_s& h0, object_proto&& p0, Header&& h, auto&& f)
    {
        fm_soft_assert(h0.id  != 0);

        const auto coord = global_coords{h0.ch->coord(), h0.tile};
        Proto p{};

        static_cast<object_proto&>(p) = move(p0);
        visit_object_proto(p, move(h), f);

        return w.make_object<Obj>(h0.id, coord, move(p));
    }

    template<typename F> void read_object(chunk* ch, F&& f)
    {
        std::shared_ptr<object> obj;
        object_id id = 0;
        auto type = object_type::none;
        local_coords tile;

        object_header_s s{
            .id = id,
            .type = type,
            .ch = ch,
            .tile = tile,
        };

        object_proto p;
        visit_object_header(p, s, f);

        switch (type)
        {
        case object_type::none:
        case object_type::COUNT:
            break;
        case object_type::critter: {
            uint16_t offset_frac = 0;
            critter_header_s h{
                .offset_frac = offset_frac,
            };
            obj = make_object<critter, critter_proto, critter_header_s>(s, move(p), move(h), f);
            goto ok;
        }
        case object_type::light:
            obj = make_object<light, light_proto, std::nullptr_t>(s, move(p), {}, f);
            goto ok;
        case object_type::scenery: {
            std::shared_ptr<scenery> objʹ;
            read_scenery(objʹ, move(p), s, f);
            obj = move(objʹ);
            goto ok;
        }
        case object_type::hole:
            fm_abort("todo! not implemented");
        }
        fm_throw("invalid object_type {}"_cf, (int)type);
ok:
        fm_assert(obj);
        fm_debug_assert(obj->c == ch);
        //non_const(obj->id) = id;
        //non_const(obj->c) = ch;
        non_const(obj->coord) = {ch->coord(), tile};

        if (PROTO >= 21) [[likely]]
            fm_soft_assert(object_counter >= id);
        else if (PROTO == 20) [[unlikely]]
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

    template<atlas_type Type> StringView get_atlas(atlasid id)
    {
        using atlas_type = typename atlas_from_type<Type>::Type;
        auto [atlas, type] = atlases[id];
        fm_soft_assert(id < atlases.size());
        fm_soft_assert(type == Type);
        const auto* atlasʹ = static_cast<const atlas_type*>(atlas);
        return atlasʹ->name();
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
            auto type = (atlas_type)s.read<std::underlying_type_t<atlas_type>>();
            atlasid id; id << s;
            auto str = get_string(id);
            void* atlas = nullptr;
            switch (type)
            {
            default: fm_throw("invalid atlas_type {}"_cf, (size_t)type);
            case atlas_type::none: break;
            case atlas_type::ground:
                atlas = loader.ground_atlas(str, loader_policy::warn).get();
                break;
            case atlas_type::wall:
                atlas = loader.wall_atlas(str, loader_policy::warn).get();
                break;
            case atlas_type::anim:
                atlas = loader.anim_atlas(str, {}, loader_policy::warn).get();
                break;
            case atlas_type::vobj:
                atlas = loader.vobj(str).atlas.get();
                break;
            }
            atlases.push_back({.atlas = atlas, .type = type });
        }
    }

    template<typename INT> void deserialize_tile_part(auto&& g, uint32_t& i, byte_reader& r)
    {
        constexpr auto highbit = reader::highbit<INT>;
        constexpr auto null = reader::null<INT>;

        INT num;
        uint32_t num_idempotent = 0;

        visit(num, r);

        if (num & highbit)
        {
            num_idempotent = num & ~highbit;
            visit(num, r);
        }

        if (num != null)
            for (uint32_t j = 0; j <= num_idempotent; j++)
                g(i+j, num);

        i += num_idempotent;
    }

    void deserialize_objects_(chunk& c, byte_reader& r)
    {
        uint32_t count;
        visit(count, r);

        for (uint32_t i = 0; i < count; i++)
        {
            using magic_type = std::decay_t<decltype(object_magic)>;
            magic_type magic;
            visit(magic, r);
            fm_soft_assert(magic == object_magic);
            (void)read_object(&c, r);
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
        visit(coord, r);
        auto& c = w[coord];

        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<atlasid>([&](uint32_t t, uint32_t id) {
                auto name = get_atlas<atlas_type::ground>(id);
                auto a = loader.ground_atlas(name, loader_policy::warn);
                c[t].ground() = { move(a), (variant_t)-1 };
            }, i, r);
        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<atlasid>([&](uint32_t t, uint32_t id) {
                auto name = get_atlas<atlas_type::wall>(id);
                auto a = loader.wall_atlas(name, loader_policy::warn);
                c[t].wall_north() = { move(a), (variant_t)-1 };
            }, i, r);
        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<atlasid>([&](uint32_t t, uint32_t id) {
                auto name = get_atlas<atlas_type::wall>(id);
                auto a = loader.wall_atlas(name, loader_policy::warn);
                c[t].wall_west() = { move(a), (variant_t)-1 };
            }, i, r);
        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<variant_t>([&](uint32_t t, variant_t id) {
                c[t].ground().variant = id;
            }, i, r);
        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<variant_t>([&](uint32_t t, variant_t id) {
                c[t].wall_north().variant = id;
            }, i, r);
        for (uint32_t i = 0; i < TILE_COUNT; i++)
            deserialize_tile_part<variant_t>([&](uint32_t t, variant_t id) {
                c[t].wall_west().variant = id;
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

template struct visitor_<reader, false>;

} // namespace

class world world::deserialize(StringView filename, loader_policy asset_policy) noexcept(false)
{
    buffer buf;

    fm_soft_assert(filename.flags() & StringViewFlag::NullTerminated);
    {
        FILE_raii f{std::fopen(filename.data(), "rb")};
        char errbuf[128];
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

        buf.data = move(buf_);
        buf.size = len;
    }

    class world w;
    struct reader r{w, asset_policy};
    r.deserialize_world(buf);

    //fm_assert("t o d o && false);
    return w;
}

} // namespace floormat

/*
NOLINTEND(
  *-missing-std-forward, *-avoid-const-or-ref-data-members,
  *-redundant-member-init, *-redundant-inline-specifier,
  *-rvalue-reference-param-not-moved, *-redundant-casting
)
*/
