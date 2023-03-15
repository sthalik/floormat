#include "entity/metadata.hpp"
#include "entity/accessor.hpp"
#include "src/scenery.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-defs.hpp"
#include "entity/types.hpp"
#include "inspect.hpp"
#include "loader/loader.hpp"
#include "chunk.hpp"
#include "chunk.inl"
#include <Corrade/Containers/ArrayViewStl.h>

//#define TEST_STR

#ifdef TEST_STR
#include <Corrade/Containers/String.h>
static Corrade::Containers::String my_str;
#endif

namespace floormat::entities {

template<>
struct entity_accessors<scenery> {
    static constexpr auto accessors()
    {
        using entity = Entity<scenery>;
        return std::tuple{
            entity::type<StringView>::field{"name"_s,
                [](const scenery& x) { return loader.strip_prefix(x.atlas->name()); },
                [](scenery&, StringView) {},
                constantly(field_status::readonly),
            },
            entity::type<rotation>::field{"rotation"_s,
                [](const scenery& x) { return x.r; },
                [](scenery& x, rotation r) { x.rotate(x.iter(), r); },
            },
            entity::type<std::uint16_t>::field{"frame"_s,
                [](const scenery& x) { return x.frame; },
                [](scenery& x, std::uint16_t value) { x.frame = value; },
                [](const scenery& x) { return constraints::range<std::uint16_t>{0, !x.atlas ? std::uint16_t(0) : std::uint16_t(x.atlas->info().nframes-1)}; }
            },
            entity::type<Vector2b>::field{"offset"_s,
                [](const scenery& x) { return x.offset; },
                [](scenery& x, Vector2b value) { x.offset = value; },
                constantly(constraints::range{Vector2b(iTILE_SIZE2/-2), Vector2b(iTILE_SIZE2/2)})
            },
            entity::type<pass_mode>::field{"pass-mode"_s,
                [](const scenery& x) { return x.pass; },
                [](scenery& x, pass_mode value) { x.chunk().with_scenery_update(x, [&] { x.pass = value; }); },
            },
            entity::type<Vector2b>::field{"bbox-offset"_s,
                [](const scenery& x) { return x.bbox_offset; },
                [](scenery& x, Vector2b value)  { x.chunk().with_scenery_update(x, [&] { x.bbox_offset = value; }); },
                [](const scenery& x) { return x.pass == pass_mode::pass ? field_status::readonly : field_status::enabled; },
            },
            entity::type<Vector2ub>::field{"bbox-size"_s,
                [](const scenery& x) { return x.bbox_size; },
                [](scenery& x, Vector2ub value) { x.chunk().with_scenery_update(x, [&] { x.bbox_size = value; }); },
                [](const scenery& x) { return x.pass == pass_mode::pass ? field_status::readonly : field_status::enabled; },
            },
            entity::type<bool>::field{"interactive"_s,
                [](const scenery& x) { return x.interactive; },
                [](scenery& x, bool value) { x.interactive = value; }
            },
#ifdef TEST_STR
            entity::type<String>::field{"string"_s,
                [](const scenery&) { return my_str; },
                [](scenery&, String value) { my_str = std::move(value); },
                constantly(constraints::max_length{8}),
            },
#endif
        };
    }
};

template<typename T, typename = void> struct has_anim_atlas : std::false_type {};
template<> struct has_anim_atlas<scenery> : std::true_type {
    static const anim_atlas& get_atlas(const scenery& x) {
        fm_assert(x.atlas);
        return *x.atlas;
    }
};

using enum_pair = std::pair<StringView, std::size_t>;
template<typename T, typename U> struct enum_values;

template<std::size_t N>
struct enum_pair_array {
    std::array<enum_pair, N> array;
    std::size_t size;
    operator ArrayView<const enum_pair>() const noexcept { return {array.data(), size};  }
};

template<std::size_t N> enum_pair_array(std::array<enum_pair, N> array, std::size_t) -> enum_pair_array<N>;

template<typename T, typename U>
requires (!std::is_enum_v<T>)
struct enum_values<T, U> : std::true_type {
    static constexpr std::array<enum_pair, 0> get() { return {}; }
};

template<typename U> struct enum_values<pass_mode, U> : std::true_type {
    static constexpr auto ret = std::to_array<enum_pair>({
        { "blocked"_s, (std::size_t)pass_mode::blocked, },
        { "see-through"_s, (std::size_t)pass_mode::see_through, },
        { "shoot-through"_s, (std::size_t)pass_mode::shoot_through, },
        { "pass"_s, (std::size_t)pass_mode::pass },
    });
    static constexpr const auto& get() { return ret; }
};

template<typename U>
requires has_anim_atlas<U>::value
struct enum_values<rotation, U> : std::false_type {
    static auto get(const U& x) {
        const anim_atlas& atlas = has_anim_atlas<U>::get_atlas(x);
        std::array<enum_pair, (std::size_t)rotation_COUNT> array;
        constexpr std::pair<StringView, rotation> values[] = {
            { "North"_s,     rotation::N  },
            { "Northeast"_s, rotation::NE },
            { "East"_s,      rotation::E  },
            { "Southeast"_s, rotation::SE },
            { "South"_s,     rotation::S  },
            { "Southwest"_s, rotation::SW },
            { "West"_s,      rotation::W  },
            { "Northwest"_s, rotation::NW },
        };
        std::size_t i = 0;
        for (auto [str, val] : values)
            if (atlas.check_rotation(val))
                array[i++] = enum_pair{str, (std::size_t)val};
        return enum_pair_array{array, i};
    }
};

template<>
bool inspect_type<scenery>(scenery& x)
{
    bool ret = false;
    visit_tuple([&](const auto& field) {
        using type = typename std::decay_t<decltype(field)>::FieldType;
        using enum_type = enum_values<type, scenery>;
        if constexpr(enum_type::value)
        {
            constexpr auto list = enum_type::get();
            ret |= inspect_field<type>(&x, field.erased(), list);
        }
        else
        {
            const auto& list = enum_type::get(x);
            ret |= inspect_field<type>(&x, field.erased(), list);
        }
    }, entity_metadata<scenery>::accessors);
    return ret;
}

} // namespace floormat::entities
