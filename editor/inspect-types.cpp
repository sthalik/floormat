#include "entity/metadata.hpp"
#include "entity/accessor.hpp"
#include "compat/limits.hpp"
#include "src/scenery.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-defs.hpp"
#include "inspect.hpp"
#include "loader/loader.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/hole.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <imgui.h>

namespace floormat::entities {

using st = field_status;

template<>
struct entity_accessors<object, inspect_intent_t> {
    static constexpr bool enable_bbox_editing(const object& x)
    {
        return x.updates_passability() || x.pass != pass_mode::pass;
    }

    static constexpr auto accessors()
    {
        using E = Entity<object>;
        return std::tuple{
            E::type<object_id>::field{"id"_s,
                &object::id,
                [](object&, object_id) {},
                constantly(st::readonly),
            },
            E::type<StringView>::field{"atlas"_s,
                [](const object& x) { return x.atlas->name(); },
                ignored_write,
                constantly(st::readonly),
            },
            E::type<rotation>::field{"rotation"_s,
                [](const object& x) { return x.r; },
                [](object& x, rotation r) { x.rotate(x.index(), r); },
            },
            E::type<uint16_t>::field{"frame"_s,
                &object::frame,
                [](object& x, uint16_t value) { x.frame = value; },
                [](const object& x) {
                    return constraints::range<uint16_t>{0, !x.atlas ? uint16_t(0) : uint16_t(x.atlas->info().nframes-1)};
                },
            },
            E::type<Vector3i>::field{"chunk"_s,
                [](const object& x) { return Vector3i(x.chunk().coord()); },
                [](object& x, Vector3i cʹ) {
                    constexpr auto cmin = Vector3i(limits<int16_t>::min, limits<int16_t>::min, chunk_z_min);
                    constexpr auto cmax = Vector3i(limits<int16_t>::max, limits<int16_t>::max, chunk_z_max);
                    auto i  = x.index();
                    auto c  = Math::clamp(cʹ, cmin, cmax);
                    auto ch = chunk_coords_{(int16_t)c.x(), (int16_t)c.y(), (int8_t)c.z()};
                    auto g0 = x.coord;
                    auto g  = global_coords{ch, x.coord.local()};
                    if (g0 != g)
                        x.teleport_to(i, g, x.offset, x.r);
                },
            },
            E::type<Vector2i>::field{"tile"_s,
                [](const object& x) {return Vector2i(x.coord.local()); },
                [](object& x, Vector2i tile) {
                    x.move_to((tile - Vector2i{x.coord.local()}) * Vector2i{tile_size_xy});
                },
            },
            E::type<Vector2i>::field{"offset"_s,
                [](const object& x) { return Vector2i(x.offset); }, // todo return Vector2b
                [](object& x, Vector2i value) { x.move_to(value - Vector2i(x.offset)); },
            },
            E::type<pass_mode>::field{"pass-mode"_s,
                &object::pass,
                [](object& x, pass_mode value) { x.set_bbox(x.offset, x.bbox_offset, x.bbox_size, value); },
            },
            E::type<Vector2b>::field{"bbox-offset"_s,
                &object::bbox_offset,
                [](object& x, Vector2b value)  { x.set_bbox(x.offset, value, x.bbox_size, x.pass); },
                [](const object& x) { return enable_bbox_editing(x) ? st::enabled : st::readonly; },
            },
            E::type<Vector2ub>::field{"bbox-size"_s,
                &object::bbox_size,
                [](object& x, Vector2ub value) { x.set_bbox(x.offset, x.bbox_offset, value, x.pass); },
                [](const object& x) { return enable_bbox_editing(x) ? st::enabled : st::readonly; },
                constantly(constraints::range<Vector2ub>{{1,1}, {255, 255}}),
            },
            E::type<bool>::field{"ephemeral"_s,
                [](const object& x) { return x.ephemeral; },
                [](object& x, bool value) { x.ephemeral = value; },
            },
        };
    }
};

template<> struct entity_accessors<generic_scenery, inspect_intent_t>
{
    static constexpr auto accessors()
    {
        using E = Entity<generic_scenery>;
        auto tuple0 = entity_accessors<object, inspect_intent_t>::accessors();
        auto tuple = std::tuple{
            E::type<bool>::field{"active"_s,
                [](const generic_scenery& x) { return x.active; },
                [](generic_scenery& x, bool b) { x.active = b; },
                constantly(st::readonly),
            },
            E::type<bool>::field{"interactive"_s,
                [](const generic_scenery& x) { return x.interactive; },
                [](generic_scenery& x, bool b) { x.interactive = b; },
                constantly(st::enabled),
            },
        };
        return std::tuple_cat(tuple0, tuple);
    }
};

template<> struct entity_accessors<door_scenery, inspect_intent_t>
{
    static constexpr auto accessors()
    {
        using E = Entity<door_scenery>;
        auto tuple0 = entity_accessors<object, inspect_intent_t>::accessors();
        auto tuple = std::tuple{
            E::type<bool>::field{"closing"_s,
                [](const door_scenery& x) { return x.closing; },
                [](door_scenery& x, bool b) { x.closing = b; },
                constantly(st::readonly),
            },
            E::type<bool>::field{"active"_s,
                [](const door_scenery& x) { return x.active; },
                [](door_scenery& x, bool b) { x.active = b; },
                constantly(st::readonly),
            },
            E::type<bool>::field{"interactive"_s,
                [](const door_scenery& x) { return x.interactive; },
                [](door_scenery& x, bool b) { x.interactive = b; },
                constantly(st::enabled),
            },
        };
        return std::tuple_cat(tuple0, tuple);
    }
};

template<> struct entity_accessors<hole, inspect_intent_t>
{
    static constexpr auto accessors()
    {
        using E = Entity<hole>;
        auto tuple0 = entity_accessors<object, inspect_intent_t>::accessors();
        auto tuple = std::tuple{
            E::type<uint8_t>::field{"height"_s,
                &hole::height,
                &hole::set_height,
                [](const hole& x) { return x.flags.is_wall ? st::enabled : st::readonly; },
            },
            E::type<uint8_t>::field{"z-offset"_s,
                &hole::z_offset,
                &hole::set_z_offset,
                [](const hole& x) { return x.flags.is_wall ? st::enabled : st::readonly; },
                constantly(constraints::range<uint8_t>{0, tile_size_z}),
            },
            E::type<bool>::field{ "enabled"_s,
                [](const hole& x) { return x.flags.enabled; },
                [](hole& x, bool value) { x.set_enabled(x.flags.on_render, x.flags.on_physics, value); },
            },
            E::type<bool>::field{"on-render"_s,
                [](const hole& x) { return x.flags.on_render; },
                [](hole& x, bool value) { x.set_enabled(value, x.flags.on_physics, x.flags.enabled); },
            },
            E::type<bool>::field{ "on-physics"_s,
                [](const hole& x) { return x.flags.on_physics; },
                [](hole& x, bool value) { x.set_enabled(x.flags.on_render, value, x.flags.enabled); },
            },
        };
        return std::tuple_cat(tuple0, tuple);
    }
};

template<typename, typename = void> struct has_anim_atlas : std::false_type {};

template<typename T>
requires requires (const T& x) { { x.atlas } -> std::convertible_to<const std::shared_ptr<anim_atlas>&>; }
struct has_anim_atlas<T> : std::true_type {
    static const anim_atlas& get_atlas(const object& x) { return *x.atlas; }
};

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
struct enum_values<T, U>
{
    static constexpr std::array<enum_pair, 0> get(const U&) { return {}; }
};

template<typename U> struct enum_values<pass_mode, U>
{
    static constexpr auto ret = std::to_array<enum_pair>({
        { "blocked"_s, (size_t)pass_mode::blocked, },
        { "see-through"_s, (size_t)pass_mode::see_through, },
        { "shoot-through"_s, (size_t)pass_mode::shoot_through, },
        { "pass"_s, (size_t)pass_mode::pass },
    });
    static constexpr const auto& get(const U&) { return ret; }
};

template<typename U>
requires has_anim_atlas<U>::value
struct enum_values<rotation, U>
{
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

template<typename T, typename Intent>
bool inspect_type(T& x, Intent)
{
    size_t width = 0;
    visit_tuple([&](const auto& field) {
        const auto& name = field.name;
        auto width_ = (size_t)ImGui::CalcTextSize(name.cbegin(), name.cend()).x;
        width = std::max(width, width_);
    }, entity_metadata<T, Intent>::accessors);

    bool ret = false;
    visit_tuple([&](const auto& field) {
        using type = typename std::decay_t<decltype(field)>::FieldType;
        using enum_type = enum_values<type, T>;
        const auto& list = enum_type::get(x);
        ret |= inspect_field<type>(&x, field.erased(), list, width);
    }, entity_metadata<T, Intent>::accessors);
    return ret;
}

template<>
struct entity_accessors<critter, inspect_intent_t> {
    static constexpr auto accessors()
    {
        using E = Entity<critter>;
        auto prev = entity_accessors<object, inspect_intent_t>::accessors();
        auto t0 = std::tuple{
            E::type<String>::field{"name"_s,
                [](const critter& x) { return x.name; },
                [](critter& x, const String& value) { x.name = value; } },
            E::type<float>::field{"speed"_s,
                [](const critter& x) { return x.speed; },
                [](critter& x, float value) { x.speed = Math::clamp(value, 0.f, 1e6f); }
            },
        };
        auto t1 = std::tuple{
            E::type<bool>::field{"playable"_s,
                [](const critter& x) { return x.playable; },
                [](critter& x, bool value) { x.playable = value; },
                constantly(constraints::max_length{ 128 }) },
        };
        return std::tuple_cat(t0, t1, prev);
    }
};

template<typename U> struct enum_values<light_falloff, U>
{
    static constexpr auto ret = std::to_array<enum_pair>({
        { "constant"_s, (size_t)light_falloff::constant },
        { "linear"_s, (size_t)light_falloff::linear },
        { "quadratic"_s, (size_t)light_falloff::quadratic },
    });
    static constexpr const auto& get(const U&) { return ret; }
};

template<>
struct entity_accessors<light, inspect_intent_t>
{
    static constexpr auto accessors()
    {
        using E = Entity<light>;
        auto tuple0 = entity_accessors<object, inspect_intent_t>::accessors();
        auto tuple = std::tuple{
            E::type<Color4ub>::field{"color"_s,
                [](const light& x) { return x.color; },
                [](light& x, Color4ub value) { x.color = value; },
                constantly(constraints::range<Color4ub>{{0, 0, 0, 0}, {255, 255, 255, 255}}),
            },
            E::type<light_falloff>::field{"falloff"_s,
                [](const light& x) { return x.falloff; },
                [](light& x, light_falloff value) { x.falloff = value; },
            },
            E::type<float>::field{"range"_s,
                [](const light& x) { return x.max_distance; },
                [](light& x, float value) { x.max_distance = value; },
            },
            E::type<bool>::field{"enabled"_s,
                [](const light& x) { return !!x.enabled; },
                [](light& x, bool value) { x.enabled = value; },
            },
        };
        return std::tuple_cat(tuple0, tuple);
    }
};

template bool inspect_type(generic_scenery&, inspect_intent_t);
template bool inspect_type(door_scenery&, inspect_intent_t);
template bool inspect_type(critter&, inspect_intent_t);
template bool inspect_type(light&, inspect_intent_t);

bool inspect_object_subtype(object& x)
{
    const auto type = x.type();
    switch (type)
    {
    case object_type::none:
    case object_type::COUNT:
        break;
    case object_type::scenery: {
        auto& sc = static_cast<scenery&>(x);
        const auto sc_type = sc.scenery_type();
        switch (sc_type)
        {
        case scenery_type::none:
        case scenery_type::COUNT:
            break;
        case scenery_type::generic:
            return inspect_type(static_cast<generic_scenery&>(sc), inspect_intent_t{});
        case scenery_type::door:
            return inspect_type(static_cast<door_scenery&>(sc), inspect_intent_t{});
        }
        fm_warn_once("unknown scenery subtype '%d'", (int)sc_type); [[fallthrough]];
    }
    case object_type::critter: return inspect_type(static_cast<critter&>(x), inspect_intent_t{});
    case object_type::light:   return inspect_type(static_cast<light&>(x), inspect_intent_t{});
    case object_type::hole:    return inspect_type(static_cast<hole&>(x), inspect_intent_t{});
    }
    fm_warn_once("unknown object subtype '%d'", (int)type);
    return false;
}

} // namespace floormat::entities
