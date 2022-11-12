#include "impl.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"

namespace floormat::loader_detail {

anim_def loader_impl::deserialize_anim(StringView filename)
{
    return json_helper::from_json<anim_def>(filename);
}

} // namespace floormat::loader_detail
