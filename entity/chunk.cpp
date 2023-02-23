#include "entity/metadata.hpp"
#include "entity/accessor.hpp"
#include "src/scenery.hpp"
#include "src/anim-atlas.hpp"
#include "src/tile-defs.hpp"

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
                [](scenery_ref& x, Vector2b value) { x.frame.offset = value; },
                constantly(constraints::range{Vector2b(iTILE_SIZE2/-2), Vector2b(iTILE_SIZE2/2)})
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

} // namespace floormat::entities
