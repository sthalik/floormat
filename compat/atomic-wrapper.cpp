#include "atomic-wrapper.hpp"

#if defined __GNUC__ && !defined __clang__
// Same -Wattributes case as src/spritebatch.cpp.
#pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace floormat {

template<atomic_type T> [[fm_always_inline]] T Atomic<T>::load(memory_order o) const noexcept { return atomic_load(&value, o); }
template<atomic_type T> [[fm_always_inline]] void Atomic<T>::store(T x, memory_order o) noexcept { atomic_store(&value, x, o); }
template<atomic_type T> [[fm_always_inline]] T Atomic<T>::exchange(T x, memory_order o) noexcept { return atomic_exchange(&value, x, o); }

template<atomic_type T>
[[fm_always_inline]] bool Atomic<T>::compare_exchange_strong(T& expected, T desired, memory_order o) noexcept
{
    return atomic_compare_exchange(&value, expected, desired, o);
}

template<atomic_type T>
[[fm_always_inline]] bool Atomic<T>::compare_exchange_weak(T& expected, T desired, memory_order o) noexcept
{
    return atomic_compare_exchange_weak(&value, expected, desired, o);
}

template<atomic_type T> [[fm_always_inline]] T Atomic<T>::fetch_add(T x, memory_order o) noexcept { return atomic_fetch_add(&value, x, o); }
template<atomic_type T> [[fm_always_inline]] T Atomic<T>::fetch_sub(T x, memory_order o) noexcept { return atomic_fetch_sub(&value, x, o); }
template<atomic_type T> [[fm_always_inline]] T Atomic<T>::fetch_and(T x, memory_order o) noexcept { return atomic_fetch_and(&value, x, o); }
template<atomic_type T> [[fm_always_inline]] T Atomic<T>::fetch_or (T x, memory_order o) noexcept { return atomic_fetch_or (&value, x, o); }
template<atomic_type T> [[fm_always_inline]] T Atomic<T>::fetch_xor(T x, memory_order o) noexcept { return atomic_fetch_xor(&value, x, o); }

template class Atomic<int8_t>;
template class Atomic<uint8_t>;
template class Atomic<int16_t>;
template class Atomic<uint16_t>;
template class Atomic<int32_t>;
template class Atomic<uint32_t>;
template class Atomic<int64_t>;
template class Atomic<uint64_t>;

} // namespace floormat
