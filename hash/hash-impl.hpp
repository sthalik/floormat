#pragma once
#include "compat/defs.hpp"

#if (defined __x86_64__ || defined __i386__ || defined _M_AMD64 || defined _M_IX86) && \
    (defined __AES__ || defined _MSC_VER && defined __AVX__)
#define FM_HASH_HAVE_MEOWHASH 1
#else
#define FM_HASH_HAVE_MEOWHASH 0
#endif

#if FM_HASH_HAVE_MEOWHASH
namespace floormat::meow_hash {
size_t hash_buf(const void* __restrict buf, size_t size) noexcept;
size_t hash_int(uint32_t x) noexcept;
size_t hash_int(uint64_t x) noexcept;
} // namespace floormat::meow_hash
#endif

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
