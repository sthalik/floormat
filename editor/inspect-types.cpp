#include "entity/metadata.hpp"
#include "entity/accessor.hpp"
#include "src/scenery.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-defs.hpp"
#include "inspect.hpp"
#include "loader/loader.hpp"
#include "chunk.hpp"
#include <Corrade/Containers/ArrayViewStl.h>

namespace floormat::entities {

template<>
struct entity_accessors<entity> {
    static constexpr auto accessors()
    {
        using E = Entity<entity>;
        return std::tuple{
            E::type<object_id>::field{"id"_s,
                [](const entity& x) { return x.id; },
                [](entity&, object_id) {},
                constantly(field_status::readonly),
            },
            E::type<StringView>::field{"atlas"_s,
                [](const entity& x) { return loader.strip_prefix(x.atlas->name()); },
                [](entity&, StringView) {},
                constantly(field_status::readonly),
            },
            E::type<rotation>::field{"rotation"_s,
                [](const entity& x) { return x.r; },
                [](entity& x, rotation r) { x.rotate(x.index(), r); },
            },
            E::type<uint16_t>::field{"frame"_s,
                [](const entity& x) { return x.frame; },
                [](entity& x, uint16_t value) { x.frame = value; },
                [](const entity& x) { return constraints::range<uint16_t>{0, !x.atlas ? uint16_t(0) : uint16_t(x.atlas->info().nframes-1)}; }
            },
            E::type<Vector2b>::field{"offset"_s,
                [](const entity& x) { return x.offset; },
                [](entity& x, Vector2b value) { x.set_bbox(value, x.bbox_offset, x.bbox_size, x.pass); },
                constantly(constraints::range{Vector2b(iTILE_SIZE2/-2), Vector2b(iTILE_SIZE2/2)})
            },
            E::type<pass_mode>::field{"pass-mode"_s,
                [](const entity& x) { return x.pass; },
                [](entity& x, pass_mode value) { x.set_bbox(x.offset, x.bbox_offset, x.bbox_size, value); }
            },
            E::type<Vector2b>::field{"bbox-offset"_s,
                [](const entity& x) { return x.bbox_offset; },
                [](entity& x, Vector2b value)  { x.set_bbox(x.offset, value, x.bbox_size, x.pass); },
                [](const entity& x) { return x.pass == pass_mode::pass ? field_status::readonly : field_status::enabled; },
            },
            E::type<Vector2ub>::field{"bbox-size"_s,
                [](const entity& x) { return x.bbox_size; },
                [](entity& x, Vector2ub value) { x.set_bbox(x.offset, x.bbox_offset, value, x.pass); },
                [](const entity& x) { return x.pass == pass_mode::pass ? field_status::readonly : field_status::enabled; },
            },
        };
    }
};

template<>
struct entity_accessors<scenery> {
    static constexpr auto accessors()
    {
        using E = Entity<scenery>;
        auto tuple0 = entity_accessors<entity>::accessors();
        auto tuple = std::tuple{
            E::type<bool>::field{"interactive"_s,
                [](const scenery& x) { return x.interactive; },
                [](scenery& x, bool value) { x.interactive = value; }
            },
        };
        return std::tuple_cat(tuple0, tuple);
    }
};

template<typename T, typename = void> struct has_anim_atlas : std::false_type {};
template<> struct has_anim_atlas<entity> : std::true_type {
    static const anim_atlas& get_atlas(const entity& x) {
        fm_assert(x.atlas);
        return *x.atlas;
    }
};
template<> struct has_anim_atlas<scenery> : has_anim_atlas<entity> {};

using enum_pair = std::pair<StringView, size_t>;
template<typename T, typename U> struct enum_values;

template<size_t N>
struct enum_pair_array {
    std::array<enum_pair, N> array;
    size_t size;
    operator ArrayView<const enum_pair>() const noexcept { return {array.data(), size};  }
};

template<size_t N> enum_pair_array(std::array<enum_pair, N> array, size_t) -> enum_pair_array<N>;

template<typename T, typename U>
requires (!std::is_enum_v<T>)
struct enum_values<T, U> : std::true_type {
    static constexpr std::array<enum_pair, 0> get() { return {}; }
};

template<typename U> struct enum_values<pass_mode, U> : std::true_type {
    static constexpr auto ret = std::to_array<enum_pair>({
        { "blocked"_s, (size_t)pass_mode::blocked, },
        { "see-through"_s, (size_t)pass_mode::see_through, },
        { "shoot-through"_s, (size_t)pass_mode::shoot_through, },
        { "pass"_s, (size_t)pass_mode::pass },
    });
    static constexpr const auto& get() { return ret; }
};

template<typename U>
requires has_anim_atlas<U>::value
struct enum_values<rotation, U> : std::false_type {
    static auto get(const U& x) {
        const anim_atlas& atlas = has_anim_atlas<U>::get_atlas(x);
        std::array<enum_pair, (size_t)rotation_COUNT> array;
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
        size_t i = 0;
        for (auto [str, val] : values)
            if (atlas.check_rotation(val))
                array[i++] = enum_pair{str, (size_t)val};
        return enum_pair_array{array, i};
    }
};

template<typename T> bool inspect_type(T& x)
{
    bool ret = false;
    visit_tuple([&](const auto& field) {
        using type = typename std::decay_t<decltype(field)>::FieldType;
        using enum_type = enum_values<type, T>;
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
    }, entity_metadata<T>::accessors);
    return ret;
}

template bool inspect_type<entity>(entity&);
template bool inspect_type<scenery>(scenery&);

} // namespace floormat::entities
