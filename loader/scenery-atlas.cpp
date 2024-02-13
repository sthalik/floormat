#include "impl.hpp"
#include "impl.hpp"
#include "atlas-loader.inl"
#include "scenery-traits.hpp"
#include "scenery-cell.hpp"
#include "src/anim.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"

namespace floormat {

anim_def loader_::deserialize_anim_def(StringView filename) noexcept(false)
{
    return json_helper::from_json<anim_def>(filename);
}

} // namespace floormat
