#include "entity/metadata.hpp"
#include "entity/accessor.hpp"
#include "src/scenery.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-defs.hpp"
#include "inspect.hpp"
#include "loader/loader.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <imgui.h>

namespace floormat::entities {

using st = field_status;

template<>
struct entity_accessors<object, inspect_intent_t> {
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
                [](object& x, Vector3i tile) {
                    if (tile.z() != x.coord.z()) // todo
                    {
                        fm_warn_once("object tried to move to different Z level (from %d to %d)", (int)tile.z(), (int)x.coord.z());
                        return;
                    }
                    auto foo1 = Vector2i{tile.x(), tile.y()};
                    auto foo2 = Vector2i{x.coord.chunk()};
                    constexpr auto chunk_size = Vector2i{tile_size_xy} * TILE_MAX_DIM;

                    x.move_to((foo1 - foo2) * chunk_size);
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
                //[](object& x, Vector2i value) { x.set_bbox(value, x.bbox_offset, x.bbox_size, x.pass); },
                [](object& x, Vector2i value) { x.move_to(value - Vector2i(x.offset)); },
                //constantly(constraints::range{Vector2b(iTILE_SIZE2/-2), Vector2b(iTILE_SIZE2/2)}),
            },
            E::type<pass_mode>::field{"pass-mode"_s,
                &object::pass,
                [](object& x, pass_mode value) { x.set_bbox(x.offset, x.bbox_offset, x.bbox_size, value); },
            },
            E::type<Vector2b>::field{"bbox-offset"_s,
                &object::bbox_offset,
                [](object& x, Vector2b value)  { x.set_bbox(x.offset, value, x.bbox_size, x.pass); },
                [](const object& x) { return x.pass == pass_mode::pass ? st::readonly : st::enabled; },
            },
            E::type<Vector2ub>::field{"bbox-size"_s,
                &object::bbox_size,
                [](object& x, Vector2ub value) { x.set_bbox(x.offset, x.bbox_offset, value, x.pass); },
                [](const object& x) { return x.pass == pass_mode::pass ? st::readonly : st::enabled; },
                constantly(constraints::range<Vector2ub>{{1,1}, {255, 255}}),
            },
            E::type<bool>::field{"ephemeral"_s,
                [](const object& x) { return x.ephemeral; },
                [](object& x, bool value) { x.ephemeral = value; },
            },
        };
    }
};

template<typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template<>
struct entity_accessors<scenery, inspect_intent_t> {
    static constexpr auto accessors()
    {
        using E = Entity<scenery>;
        auto tuple0 = entity_accessors<object, inspect_intent_t>::accessors();
        auto tuple = std::tuple{
            E::type<bool>::field{"interactive"_s,
                                 [](const scenery& x) {
                                   return std::visit(overloaded {
                                       [&](const door_scenery& s) { return s.interactive; },
                                       [&](const generic_scenery& s) { return s.interactive; },
                                   }, x.subtype);
                                 },
                                 [](scenery& x, bool b) {
                                   return std::visit(overloaded {
                                       [&](door_scenery& s) { s.interactive = b; },
                                       [&](generic_scenery& s) { s.interactive = b; },
                                   }, x.subtype);
                                 },
                                [](const scenery& x) {
                                  return std::visit(overloaded {
                                      [&](const door_scenery&) { return st::enabled; },
                                      [&](const generic_scenery&) { return st::enabled; },
                                      //[](auto&&) { return st::hidden; },
                                  }, x.subtype);
                                },
            },
        };
        return std::tuple_cat(tuple0, tuple);
    }
};

template<typename T, typename = void> struct has_anim_atlas : std::false_type {};

template<typename T>
requires requires (const T& x) { { x.atlas } -> std::convertible_to<const std::shared_ptr<anim_atlas>&>; }
struct has_anim_atlas<T> : std::true_type {
    static const anim_atlas& get_atlas(const object& x) { return *x.atlas; }
};

#if 0
template<> struct has_anim_atlas<object> : std::true_type {
    static const anim_atlas& get_atlas(const object& x) { return *x.atlas; }
};
template<> struct has_anim_atlas<scenery> : has_anim_atlas<object> {};
template<> struct has_anim_atlas<critter> : has_anim_atlas<object> {};
#endif

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
static bool inspect_type(T& x, Intent)
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

//template bool inspect_type(object&);
template bool inspect_type(scenery&, inspect_intent_t);
template bool inspect_type(critter&, inspect_intent_t);
template bool inspect_type(light&, inspect_intent_t);

bool inspect_object_subtype(object& x)
{
    switch (auto type = x.type())
    {
    default: fm_warn_once("unknown object subtype '%d'", (int)type); return false;
    //case object_type::none: return inspect_type(x);
    case object_type::scenery: return inspect_type(static_cast<scenery&>(x), inspect_intent_t{});
    case object_type::critter: return inspect_type(static_cast<critter&>(x), inspect_intent_t{});
    case object_type::light: return inspect_type(static_cast<light&>(x), inspect_intent_t{});
    }
}

} // namespace floormat::entities
