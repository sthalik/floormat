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
struct entity_accessors<scenery_ref> {
    static constexpr auto accessors()
    {
        using entity = Entity<scenery_ref>;
        using frame_t = scenery::frame_t;
        return std::tuple{
            entity::type<StringView>::field{"name"_s,
                [](const scenery_ref& x) { return loader.strip_prefix(x.atlas->name()); },
                [](scenery_ref&, StringView) {},
                constantly(field_status::readonly),
            },
            entity::type<scenery::frame_t>::field{"frame"_s,
                [](const scenery_ref& x) { return x.frame.frame; },
                [](scenery_ref& x, frame_t value) { x.frame.frame = value; },
                [](const scenery_ref& x) { return constraints::range<frame_t>{0, !x.atlas ? frame_t(0) : frame_t(x.atlas->info().nframes-1)}; }
            },
            entity::type<Vector2b>::field{"offset"_s,
                [](const scenery_ref& x) { return x.frame.offset; },
                [](scenery_ref& x, Vector2b value) { x.frame.offset = value; },
                constantly(constraints::range{Vector2b(iTILE_SIZE2/-2), Vector2b(iTILE_SIZE2/2)})
            },
            entity::type<pass_mode>::field{"pass-mode"_s,
                [](const scenery_ref& x) { return x.frame.passability; },
                [](scenery_ref& x, pass_mode value) {
                    x.chunk().with_scenery_bbox_update(x.index(), [&] {
                      x.frame.passability = value;
                    });
                },
            },
            entity::type<Vector2b>::field{"bbox-offset"_s,
                [](const scenery_ref& x) { return x.frame.bbox_offset; },
                [](scenery_ref& x, Vector2b value)  {
                    x.chunk().with_scenery_bbox_update(x.index(), [&] {
                        x.frame.bbox_offset = value;
                    });
                },
                [](const scenery_ref& x) {
                    return x.frame.passability == pass_mode::pass
                           ? field_status::readonly
                           : field_status::enabled;
                },
            },
            entity::type<Vector2ub>::field{"bbox-size"_s,
                [](const scenery_ref& x) { return x.frame.bbox_size; },
                [](scenery_ref& x, Vector2ub value) {
                    x.chunk().with_scenery_bbox_update(x.index(), [&] {
                        x.frame.bbox_size = value;
                    });
                },
                [](const scenery_ref& x) { return x.frame.passability == pass_mode::pass ? field_status::readonly : field_status::enabled; },
            },
            entity::type<bool>::field{"interactive"_s,
                [](const scenery_ref& x) { return x.frame.interactive; },
                [](scenery_ref& x, bool value) { x.frame.interactive = value; }
            },
#ifdef TEST_STR
            entity::type<String>::field{"string"_s,
                [](const scenery_ref&) { return my_str; },
                [](scenery_ref&, String value) { my_str = std::move(value); }
            },
#endif
        };
    }
};

using enum_pair = std::pair<StringView, std::size_t>;

template<typename T> constexpr auto enum_values();
template<typename T> requires (!std::is_enum_v<T>) constexpr std::array<enum_pair, 0> enum_values(){ return {}; }

template<>
constexpr auto enum_values<pass_mode>()
{
    return std::to_array<enum_pair>({
        { "blocked"_s, (std::size_t)pass_mode::blocked, },
        { "see-through"_s, (std::size_t)pass_mode::see_through, },
        { "shoot-through"_s, (std::size_t)pass_mode::shoot_through, },
        { "pass"_s, (std::size_t)pass_mode::pass },
    });
}

template<>
bool inspect_type<scenery_ref>(scenery_ref& x)
{
    bool ret = false;
    visit_tuple([&](const auto& field) {
        using type = typename std::decay_t<decltype(field)>::FieldType;
        constexpr auto list = enum_values<type>();
        ret |= inspect_field<type>(&x, field.erased(), list);
    }, entity_metadata<scenery_ref>::accessors);
    return ret;
}

} // namespace floormat::entities
