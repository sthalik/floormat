#pragma once

#if (defined __x86_64__ || defined __i386__ || defined _M_AMD64 || defined _M_IX86) && \
    (defined __AES__ || defined _MSC_VER && !defined __clang__ && defined __AVX__)
#define FM_HASH_HAVE_MEOWHASH 1
#else
#define FM_HASH_HAVE_MEOWHASH 0
#endif

#if FM_HASH_HAVE_MEOWHASH
namespace floormat::meow_hash {
usize hash_buf(const void* __restrict buf, usize size) noexcept;
} // namespace floormat::meow_hash
#endif

namespace floormat::xxHash {
usize hash_buf(const void* __restrict buf, usize size) noexcept;
} // namespace floormat::xxHash

namespace floormat::SipHash {
usize hash_buf(const void* __restrict buf, usize size) noexcept;
} // namespace floormat::SipHash

namespace floormat::MurmurHash {
usize hash_buf(const void* __restrict buf, usize size) noexcept;
} // namespace floormat::MurmurHash
