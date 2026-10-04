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

template<typename T>
concept atomic_type = std::same_as<T, int8_t>  || std::same_as<T, uint8_t>  ||
                      std::same_as<T, int16_t> || std::same_as<T, uint16_t> ||
                      std::same_as<T, int32_t> || std::same_as<T, uint32_t> ||
                      std::same_as<T, int64_t> || std::same_as<T, uint64_t>;

// 64-bit values must be 8-byte aligned. i386 SysV only aligns them to 4 by default.
// Definitions live in atomic.cpp and are explicitly instantiated for each atomic_type.
// With LTO, calls get inlined and the memory order becomes a constant.

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

} // namespace floormat
