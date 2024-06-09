#pragma once

namespace floormat::xxHash {
size_t hash_buf(const void* __restrict buf, size_t size) noexcept;
size_t hash_int(uint32_t x) noexcept;
size_t hash_int(uint64_t x) noexcept;
} // namespace floormat::xxHash

namespace floormat::SipHash {
size_t hash_buf(const void* __restrict buf, size_t size) noexcept;
size_t hash_int(uint32_t x) noexcept;
size_t hash_int(uint64_t x) noexcept;
} // namespace floormat::SipHash

namespace floormat::MurmurHash {
size_t hash_buf(const void* __restrict buf, size_t size) noexcept;
size_t hash_int(uint32_t x) noexcept;
size_t hash_int(uint64_t x) noexcept;
} // namespace floormat::MurmurHash

namespace floormat::FNVHash {
size_t hash_buf(const void* __restrict buf, size_t size) noexcept;
size_t hash_int(uint32_t x) noexcept;
size_t hash_int(uint64_t x) noexcept;
} // namespace floormat::FNVHash
