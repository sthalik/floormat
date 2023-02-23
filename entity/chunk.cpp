#include "entity/metadata.hpp"
#include "entity/accessor.hpp"
#include "src/scenery.hpp"
#include "src/anim-atlas.hpp"

namespace floormat::entities {

template<> struct entity_accessors<scenery_ref> {
    static constexpr auto accessors()
    {
        using entity = Entity<scenery_ref>;
        using frame_t = scenery::frame_t;
        constexpr auto tuple = std::make_tuple(
            entity::type<scenery::frame_t>::field{"frame",
                [](const scenery_ref& x) { return x.frame.frame; },
                [](scenery_ref& x, frame_t value) { x.frame.frame = value; },
                [](const scenery_ref& x) { return constraints::range<frame_t>{0, !x.atlas ? frame_t(0) : frame_t(x.atlas->info().nframes)}; }
            }
        );
        return tuple;
    }
};

} // namespace floormat::entities
