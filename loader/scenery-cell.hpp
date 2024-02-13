#pragma once
#include "compat/vector-wrapper-fwd.hpp"
#include "compat/safe-ptr.hpp"
#include "src/scenery.hpp"
#include <memory>
#include <cr/String.h>
#include <cr/Optional.h>

namespace floormat {

struct json_wrapper;
struct scenery_proto;

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy-with-user-provided-dtor"
#endif

struct scenery_cell final
{
    String name;
    safe_ptr<json_wrapper> data{make_json_wrapper()};
    Optional<scenery_proto> proto;

    ~scenery_cell() noexcept;
    static vector_wrapper<const scenery_cell> load_atlases_from_json();
    [[nodiscard]] static json_wrapper* make_json_wrapper();
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

} // namespace floormat
