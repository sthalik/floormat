#pragma once
#include "compat/multi-level-table.hpp"
#include "compat/borrowed-ptr.hpp"

namespace floormat {

struct object;

constexpr inline mlt_params object_table_params = mlt_params{
    .levels = { {.bits = 17}, {.bits = 18, .dynamic = true}, },
    .top_source = mlt_source::superpage,
    .page_source = mlt_source::superpage,
    .free_empty = true,
}.validate();

using object_table = multi_level_table<bptr<object>, object_table_params>;

} // namespace floormat
