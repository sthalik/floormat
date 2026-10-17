#pragma once
#include "compat/integer-types.hpp"
#include <concepts>
#include <type_traits>

namespace floormat {

#ifdef __ATOMIC_RELAXED
enum class memory_order : int {
    relaxed = __ATOMIC_RELAXED, acquire = __ATOMIC_ACQUIRE, release = __ATOMIC_RELEASE,
    acq_rel = __ATOMIC_ACQ_REL, seq_cst = __ATOMIC_SEQ_CST,
};
#else
// Copied from MSVC STL's _Atomic_memory_order_*, declared only in <atomic>. Checked by test/memory-order.cpp.
enum class memory_order : int { relaxed = 0, acquire = 2, release = 3, acq_rel = 4, seq_cst = 5, };
#endif

// The fixed-width aliases miss some types: on macOS, uintptr_t and size_t are unsigned long
// and uint64_t is unsigned long long.
template<typename T>
concept atomic_type = std::same_as<T, signed char> || std::same_as<T, unsigned char>      ||
                      std::same_as<T, short>       || std::same_as<T, unsigned short>     ||
                      std::same_as<T, int>         || std::same_as<T, unsigned>           ||
                      std::same_as<T, long>        || std::same_as<T, unsigned long>      ||
                      std::same_as<T, long long>   || std::same_as<T, unsigned long long>;

// 64-bit values must be 8-byte aligned. i386 SysV only aligns them to 4 by default.
// Definitions live in atomic.cpp and are explicitly instantiated for each atomic_type.
// With LTO, calls get inlined and the memory order becomes a constant.
// Without it, GCC treats every order as seq_cst.

template<atomic_type T> [[nodiscard]] T atomic_load(const volatile T* p, memory_order o = memory_order::seq_cst) noexcept;
template<atomic_type T> void atomic_store(volatile T* p, std::type_identity_t<T> x, memory_order o = memory_order::seq_cst) noexcept;
template<atomic_type T> T atomic_exchange(volatile T* p, std::type_identity_t<T> x, memory_order o = memory_order::seq_cst) noexcept;

// On failure, `expected` is updated to the current value.
template<atomic_type T> bool atomic_compare_exchange(volatile T* p, T& expected, std::type_identity_t<T> desired, memory_order o = memory_order::seq_cst, bool weak = false) noexcept;
template<atomic_type T> bool atomic_compare_exchange_weak(volatile T* p, T& expected, std::type_identity_t<T> desired, memory_order o = memory_order::seq_cst) noexcept;

template<atomic_type T> T atomic_fetch_add(volatile T* p, std::type_identity_t<T> x, memory_order o = memory_order::seq_cst) noexcept;
template<atomic_type T> T atomic_fetch_sub(volatile T* p, std::type_identity_t<T> x, memory_order o = memory_order::seq_cst) noexcept;
template<atomic_type T> T atomic_fetch_and(volatile T* p, std::type_identity_t<T> x, memory_order o = memory_order::seq_cst) noexcept;
template<atomic_type T> T atomic_fetch_or (volatile T* p, std::type_identity_t<T> x, memory_order o = memory_order::seq_cst) noexcept;
template<atomic_type T> T atomic_fetch_xor(volatile T* p, std::type_identity_t<T> x, memory_order o = memory_order::seq_cst) noexcept;

void atomic_thread_fence(memory_order o = memory_order::seq_cst) noexcept;
void cpu_relax() noexcept;

#if defined __linux__ && !defined FM_NO_PLATFORM_ATOMIC_WAIT
constexpr inline uint32_t atomic_wait_sizes[] = { 4 };
#elif defined __FreeBSD__ && !defined FM_NO_PLATFORM_ATOMIC_WAIT && defined __LP64__
constexpr inline uint32_t atomic_wait_sizes[] = { 4, 8 };
#elif defined __FreeBSD__ && !defined FM_NO_PLATFORM_ATOMIC_WAIT
constexpr inline uint32_t atomic_wait_sizes[] = { 4 };
#elif defined __APPLE__ && !defined FM_NO_PLATFORM_ATOMIC_WAIT && defined __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 140400
constexpr inline uint32_t atomic_wait_sizes[] = { 4, 8 };
#else
constexpr inline uint32_t atomic_wait_sizes[] = { 1, 2, 4, 8 };
#endif

consteval bool atomic_wait_size_supported(size_t size)
{
    for (auto x : atomic_wait_sizes)
        if (x == size)
            return true;
    return false;
}

template<typename T>
concept atomic_wait_type = atomic_type<T> && atomic_wait_size_supported(sizeof(T));

// The pool's notify_one wakes every waiter in the bucket.
template<atomic_wait_type T> void atomic_wait(const volatile T* p, std::type_identity_t<T> old, memory_order o = memory_order::seq_cst) noexcept;
template<atomic_wait_type T> void atomic_notify_one(const volatile T* p) noexcept;
template<atomic_wait_type T> void atomic_notify_all(const volatile T* p) noexcept;

} // namespace floormat
