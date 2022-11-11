#include "impl.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"

namespace floormat::loader_detail {

Serialize::anim deserialize_anim(StringView filename)
{
    return json_helper::from_json<Serialize::anim>(filename);
}

} // namespace floormat::loader_detail
