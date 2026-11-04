#pragma once

namespace floormat::Hash {

constexpr inline u64 CRC64_INITIALIZER = 0x0000000000000000ULL;

u64 crc64_update(u64 crc, const void* ptr, const usize count);

} // namespace floormat::Hash
