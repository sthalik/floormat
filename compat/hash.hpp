#pragma once

namespace floormat {

size_t hash_buf(const void* __restrict buf, size_t size) noexcept;
size_t hash_int(uint32_t x) noexcept;
size_t hash_int(uint64_t x) noexcept;

struct hash_string_view { size_t operator()(StringView str) const noexcept; };

} // namespace floormat
