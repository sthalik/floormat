#include "entity/metadata.hpp"
#include "entity/accessor.hpp"
#include "src/scenery.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-defs.hpp"
#include "entity/types.hpp"
#include "inspect.hpp"
#include <Corrade/Containers/ArrayViewStl.h>

namespace floormat::entities {

template<> struct entity_accessors<scenery_ref> {
    static constexpr auto accessors()
    {
        using entity = Entity<scenery_ref>;
        using frame_t = scenery::frame_t;
        constexpr auto tuple = std::make_tuple(
            entity::type<scenery::frame_t>::field{"frame"_s,
                [](const scenery_ref& x) { return x.frame.frame; },
                [](scenery_ref& x, frame_t value) { x.frame.frame = value; },
                [](const scenery_ref& x) { return constraints::range<frame_t>{0, !x.atlas ? frame_t(0) : frame_t(x.atlas->info().nframes)}; }
            },
            entity::type<Vector2b>::field{"offset"_s,
                [](const scenery_ref& x) { return x.frame.offset; },
                [](scenery_ref& x, Vector2b value) { x.frame.offset = value; }
                //constantly(constraints::range{Vector2b(iTILE_SIZE2/-2), Vector2b(iTILE_SIZE2/2)})
            },
            entity::type<pass_mode>::field{"pass-mode"_s,
                [](const scenery_ref& x) { return x.frame.passability; },
                [](scenery_ref& x, pass_mode value) { x.frame.passability = value; }
            },
            // todo pass_mode enum
            entity::type<bool>::field{"interactive"_s,
                [](const scenery_ref& x) { return x.frame.interactive; },
                [](scenery_ref& x, bool value) { x.frame.interactive = value; }
            }
        );
        return tuple;
    }
};

using enum_pair = std::pair<StringView, std::size_t>;

template<typename T> constexpr auto enum_values();
template<typename T> requires (!std::is_enum_v<T>) constexpr std::array<enum_pair, 0> enum_values(){ return {}; }

template<> constexpr auto enum_values<pass_mode>()
{
    return std::to_array<enum_pair>({
        { "blocked"_s, (std::size_t)pass_mode::blocked, },
        { "see-through"_s, (std::size_t)pass_mode::see_through, },
        { "shoot-through"_s, (std::size_t)pass_mode::shoot_through, },
        { "pass"_s, (std::size_t)pass_mode::pass },
    });
}

template<>
void inspect_type<scenery_ref>(scenery_ref& x)
{
    visit_tuple([&](const auto& field) {
        using type = typename std::decay_t<decltype(field)>::FieldType;
        constexpr auto list = enum_values<type>();
        auto view = ArrayView<const enum_pair>{list.data(), list.size()};
        inspect_field<type>(&x, field.erased(), view);
    }, entity_metadata<scenery_ref>::accessors);
}

} // namespace floormat::entities