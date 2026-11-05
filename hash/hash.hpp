#pragma once
#include "hash-impl.hpp"
#include <cr/StringView.h>

namespace floormat {

using xxHash::hash_buf;

struct hash_string_view { [[nodiscard]] CORRADE_ALWAYS_INLINE usize operator()(StringView str) const noexcept; };
usize hash_string_view::operator()(StringView str) const noexcept { return hash_buf(str.data(), str.size()); }

} // namespace floormat
