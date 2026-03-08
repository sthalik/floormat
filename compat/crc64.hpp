#pragma once

namespace floormat::Hash {

constexpr inline uint64_t CRC64_INITIALIZER = 0x0000000000000000ULL;

uint64_t crc64_update(uint64_t crc, const void* ptr, const size_t count);

} // namespace floormat::Hash
