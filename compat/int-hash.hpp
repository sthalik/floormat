#pragma once

// todo rename to hash-fnv.hpp

namespace floormat::Hash {

template<size_t N = sizeof nullptr * 8> struct fnvhash_params;
template<> struct fnvhash_params<32> { static constexpr uint32_t a = 0x811c9dc5u, b = 0x01000193u; };
template<> struct fnvhash_params<64> { static constexpr uint64_t a = 0xcbf29ce484222325u, b = 0x100000001b3u; };

constexpr inline size_t fnvhash_seed = fnvhash_params<>::a;

size_t fnvhash_buf(const void* __restrict buf, size_t size, size_t seed = fnvhash_seed) noexcept;

} // namespace floormat::Hash

namespace floormat { // todo

uint64_t hash_64(const void* buf, size_t size) noexcept;
uint32_t hash_32(const void* buf, size_t size) noexcept;
size_t hash_buf(const void* buf, size_t size) noexcept;

size_t hash_int(uint32_t x) noexcept;
size_t hash_int(uint64_t x) noexcept;

struct hash_string_view { size_t operator()(StringView str) const noexcept; };

} // namespace floormat
