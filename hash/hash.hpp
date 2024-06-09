#pragma once
#include "hash-impl.hpp"
#include <cr/StringView.h>

namespace floormat {

namespace Hash_int = xxHash;
namespace Hash_str = xxHash;

using Hash_int::hash_int;
using Hash_str::hash_buf;

struct hash_string_view { [[nodiscard]] CORRADE_ALWAYS_INLINE size_t operator()(StringView str) const noexcept; };
size_t hash_string_view::operator()(StringView str) const noexcept { return Hash_str::hash_buf(str.data(), str.size()); }

} // namespace floormat
