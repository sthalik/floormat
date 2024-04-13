#pragma once
#include "compat/safe-ptr.hpp"
#include "src/scenery-proto.hpp"
#include <cr/String.h>
#include <cr/Optional.h>

namespace floormat {

struct json_wrapper;
struct scenery_proto;

struct scenery_cell final
{
    String name;
    safe_ptr<json_wrapper> data{make_json_wrapper()};
    Optional<scenery_proto> proto;

    static Array<scenery_cell> load_atlases_from_json();
    [[nodiscard]] static json_wrapper* make_json_wrapper();
};

} // namespace floormat
