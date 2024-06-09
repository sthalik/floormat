#pragma once
#include "hash-impl.hpp"
#include <cr/StringView.h>

namespace floormat {
namespace Hash = xxHash;

using Hash::hash_buf;
using Hash::hash_int;

struct hash_string_view { [[nodiscard]] CORRADE_ALWAYS_INLINE size_t operator()(StringView str) const noexcept; };
size_t hash_string_view::operator()(StringView str) const noexcept { return hash_buf(str.data(), str.size()); }

} // namespace floormat
