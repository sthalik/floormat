#pragma once
#include "atomic.hpp"
#include "defs.hpp"

namespace floormat {

template<atomic_type T>
class Atomic final
{
    alignas(sizeof(T)) volatile T value;

public:
    constexpr Atomic() noexcept : value{} {}
    constexpr Atomic(T x) noexcept : value{x} {}
    fm_DISABLE_MOVE_COPY(Atomic);

    [[nodiscard]] T load(memory_order o = memory_order::seq_cst) const noexcept;
    void store(T x, memory_order o = memory_order::seq_cst) noexcept;
    T exchange(T x, memory_order o = memory_order::seq_cst) noexcept;

    // On failure, `expected` is updated to the current value.
    bool compare_exchange_strong(T& expected, T desired, memory_order o = memory_order::seq_cst) noexcept;
    bool compare_exchange_weak(T& expected, T desired, memory_order o = memory_order::seq_cst) noexcept;

    T fetch_add(T x, memory_order o = memory_order::seq_cst) noexcept;
    T fetch_sub(T x, memory_order o = memory_order::seq_cst) noexcept;
    T fetch_and(T x, memory_order o = memory_order::seq_cst) noexcept;
    T fetch_or (T x, memory_order o = memory_order::seq_cst) noexcept;
    T fetch_xor(T x, memory_order o = memory_order::seq_cst) noexcept;
};

} // namespace floormat
