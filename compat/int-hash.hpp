#pragma once

namespace floormat {

size_t int_hash(uint32_t x) noexcept;
size_t int_hash(uint64_t x) noexcept;

uint64_t fnvhash_64(const void* buf, size_t size);
uint64_t fnvhash_32(const void* buf, size_t size);

} // namespace floormat
