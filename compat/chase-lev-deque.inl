#pragma once
#include "compat/chase-lev-deque.hpp"

namespace floormat {

inline void chase_lev_no_hooks::after_array_load() noexcept {}

inline volatile uintptr_t& ChaseLevBufferPool::buffer::operator[](int64_t i) noexcept
{
    return reinterpret_cast<volatile uintptr_t*>(this + 1)[i & mask];
}

inline ChaseLevBufferPool::~ChaseLevBufferPool() noexcept
{
    for (auto& head : _heads)
        for (auto* b = (buffer*)head; b; )
        {
            auto* next = b->next;
            ::operator delete(b, std::align_val_t{alignof(buffer)});
            b = next;
        }
}

inline auto ChaseLevBufferPool::get(uint32_t log2_capacity) noexcept -> buffer*
{
    fm_assert(log2_capacity < max_log2_capacity);
    volatile uintptr_t* head = &_heads[log2_capacity];
    // Taking the whole list avoids ABA: a CAS from b to b->next could succeed after b was
    // taken and put back with another successor.
    auto* b = (buffer*)atomic_exchange(head, uintptr_t{0}, memory_order::acquire);
    if (!b)
    {
        const auto capacity = int64_t{1} << log2_capacity;
        void* p = ::operator new(sizeof(buffer) + (size_t)capacity * sizeof(uintptr_t), std::align_val_t{alignof(buffer)});
        return new (p) buffer{capacity - 1, nullptr};
    }
    if (b->next)
        push(head, b->next);
    // Pairs with the acquire slot load in steal(): a thief of b's previous deque that reads
    // anything the new owner writes into b also sees its own deque's newer _array.
    atomic_thread_fence(memory_order::release);
    return b;
}

inline void ChaseLevBufferPool::put(buffer* b) noexcept
{
    b->next = nullptr;
    push(&_heads[std::bit_width((uint64_t)b->mask)], b);
}

inline void ChaseLevBufferPool::push(volatile uintptr_t* head, buffer* first) noexcept
{
    buffer* last = first;
    while (last->next)
        last = last->next;
    uintptr_t h = atomic_load(head, memory_order::relaxed);
    do
        last->next = (buffer*)h;
    while (!atomic_compare_exchange_weak(head, h, (uintptr_t)first, memory_order::release));
}

template<typename T, typename Hooks>
auto ChaseLevDeque<T, Hooks>::grow(buffer* a, int64_t t, int64_t b) noexcept -> buffer*
{
    buffer* n = _pool.get((uint32_t)std::bit_width((uint64_t)a->mask) + 1);
    for (int64_t i = t; i < b; ++i)
        atomic_store(&(*n)[i], atomic_load(&(*a)[i], memory_order::relaxed), memory_order::relaxed);
    // Release, unlike the paper's relaxed store: a thief that loads a reused n must see the
    // copy, not slots left by n's previous deque.
    atomic_store(&_array, (uintptr_t)n, memory_order::release);
    // Only after the store, since steal() takes an unchanged _array as proof that a isn't reused.
    _pool.put(a);
    return n;
}

template<typename T, typename Hooks>
void ChaseLevDeque<T, Hooks>::store_bottom(int64_t b) noexcept
{
    atomic_store(&_bottom, b, fm_TSAN ? memory_order::release : memory_order::relaxed);
}

template<typename T, typename Hooks>
ChaseLevDeque<T, Hooks>::ChaseLevDeque(ChaseLevBufferPool& pool, uint32_t log2_capacity) noexcept :
    _array{(uintptr_t)pool.get(log2_capacity)},
    _pool{pool}
{
}

template<typename T, typename Hooks>
ChaseLevDeque<T, Hooks>::~ChaseLevDeque() noexcept
{
    _pool.put((buffer*)_array);
}

template<typename T, typename Hooks>
void ChaseLevDeque<T, Hooks>::push(T* x) noexcept
{
    fm_debug_assert(x);
    int64_t b = atomic_load(&_bottom, memory_order::relaxed);
    int64_t t = atomic_load(&_top, memory_order::acquire);
    auto* a = (buffer*)atomic_load(&_array, memory_order::relaxed);
    if (b - t > a->mask) [[unlikely]]
        a = grow(a, t, b);
    atomic_store(&(*a)[b], (uintptr_t)x, memory_order::relaxed);
    if constexpr (!fm_TSAN)
        atomic_thread_fence(memory_order::release);
    store_bottom(b + 1);
}

template<typename T, typename Hooks>
T* ChaseLevDeque<T, Hooks>::take() noexcept
{
    int64_t b = atomic_load(&_bottom, memory_order::relaxed) - 1;
    auto* a = (buffer*)atomic_load(&_array, memory_order::relaxed);
    T* x = nullptr;
    int64_t t;
    if constexpr (fm_TSAN)
    {
        atomic_store(&_bottom, b, memory_order::seq_cst);
        t = atomic_load(&_top, memory_order::seq_cst);
    }
    else
    {
        atomic_store(&_bottom, b, memory_order::relaxed);
        atomic_thread_fence(memory_order::seq_cst);
        t = atomic_load(&_top, memory_order::relaxed);
    }
    if (t <= b)
    {
        x = (T*)atomic_load(&(*a)[b], memory_order::relaxed);
        if (t == b)
        {
            if (!atomic_compare_exchange(&_top, t, t + 1, memory_order::seq_cst))
                x = nullptr;
            store_bottom(b + 1);
        }
    }
    else
        store_bottom(b + 1);
    return x;
}

template<typename T, typename Hooks>
T* ChaseLevDeque<T, Hooks>::steal() noexcept
{
    int64_t t, b;
    if constexpr (fm_TSAN)
    {
        t = atomic_load(&_top, memory_order::seq_cst);
        b = atomic_load(&_bottom, memory_order::seq_cst);
    }
    else
    {
        t = atomic_load(&_top, memory_order::acquire);
        atomic_thread_fence(memory_order::seq_cst);
        b = atomic_load(&_bottom, memory_order::acquire);
    }
    if (t >= b)
        return nullptr;

    auto* a = (buffer*)atomic_load(&_array, memory_order::acquire);
    Hooks::after_array_load();
    // Read before the CAS: once top moves past t, push() may reuse the slot. Acquire keeps
    // the _array check below after this read.
    auto* x = (T*)atomic_load(&(*a)[t], memory_order::acquire);
    // A newer _array means a may be back in the pool and refilled by another deque.
    if (atomic_load(&_array, memory_order::relaxed) != (uintptr_t)a)
        return nullptr;
    bool won = atomic_compare_exchange(&_top, t, t + 1, memory_order::seq_cst);
    return won ? x : nullptr;
}

} // namespace floormat
