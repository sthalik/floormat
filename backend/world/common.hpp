#pragma once

#include <tuple>

namespace world_detail {

using Len = int;
using Len2 = std::tuple<Len, Len>;
using Ord = int;

using Global = int;
using Global2 = std::tuple<Global, Global>;

using Abs = std::tuple<Global, Global, Len, Len>;

static constexpr inline Len BLOCK_LEN = 8;
static constexpr inline Ord BLOCK_AREA = BLOCK_LEN * BLOCK_LEN;

} // ns world_detail
