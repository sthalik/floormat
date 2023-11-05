#pragma once

namespace floormat::impl_hash {

template<size_t N> size_t hash_buf(const void* buf, size_t size) noexcept = delete;

} // namespace floormat::impl_hash

namespace floormat {

uint64_t hash_64(const void* buf, size_t size) noexcept;
uint32_t hash_32(const void* buf, size_t size) noexcept;

size_t hash_int(uint32_t x) noexcept;
size_t hash_int(uint64_t x) noexcept;

} // namespace floormat
