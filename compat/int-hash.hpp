#pragma once

namespace floormat {

size_t int_hash32(uint32_t x) noexcept;
size_t int_hash64(uint64_t x) noexcept;

inline size_t int_hash(uint32_t x) noexcept { return int_hash32(x); }
inline size_t int_hash(uint64_t x) noexcept { return int_hash64(x); }

} // namespace floormat
