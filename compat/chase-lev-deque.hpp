#pragma once
#include "compat/assert.hpp"
#include "compat/atomic.hpp"
#include "compat/defs.hpp"
#include <bit>
#include <new>

namespace floormat {

// steal() calls this between loading the array and reading its slot. Tests pause a thief there
// so that the array goes back to the pool and another deque refills it inside that window.
struct chase_lev_no_hooks
{
    static void after_array_load() noexcept {}
};

// Arrays are freed only by the destructor, so a thief holding a stale one reads valid memory.
class ChaseLevBufferPool final
{
public:
    struct alignas(64) buffer
    {
        int64_t mask;
        buffer* next;

        volatile uintptr_t& operator[](int64_t i) noexcept
        {
            return reinterpret_cast<volatile uintptr_t*>(this + 1)[i & mask];
        }
    };

    ChaseLevBufferPool() noexcept = default;

    // Every deque using the pool must be destroyed first.
    ~ChaseLevBufferPool() noexcept
    {
        for (auto& head : _heads)
            for (auto* b = (buffer*)head; b; )
            {
                auto* next = b->next;
                ::operator delete(b, std::align_val_t{alignof(buffer)});
                b = next;
            }
    }

    fm_DISABLE_MOVE_COPY(ChaseLevBufferPool);

    buffer* get(uint32_t log2_capacity) noexcept
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

    void put(buffer* b) noexcept
    {
        b->next = nullptr;
        push(&_heads[std::bit_width((uint64_t)b->mask)], b);
    }

private:
    static constexpr uint32_t max_log2_capacity = 62;

    static void push(volatile uintptr_t* head, buffer* first) noexcept
    {
        buffer* last = first;
        while (last->next)
            last = last->next;
        uintptr_t h = atomic_load(head, memory_order::relaxed);
        do
            last->next = (buffer*)h;
        while (!atomic_compare_exchange_weak(head, h, (uintptr_t)first, memory_order::release));
    }

    volatile uintptr_t _heads[max_log2_capacity] = {};
};

// One owner calls push() and take(), any number of thieves call steal(). Both return nullptr
// when empty or on a lost race. Only the owner may push. Any other thread that makes jobs
// needs its own deque or a separate multi-producer queue.
template<typename T, typename Hooks = chase_lev_no_hooks>
class ChaseLevDeque final
{
    using buffer = ChaseLevBufferPool::buffer;

    alignas(64) volatile int64_t _top = 0;
    alignas(64) volatile int64_t _bottom = 0;
    // Own line: steal() loads it twice, and the owner stores to _bottom on every push and take.
    alignas(64) volatile uintptr_t _array;
    ChaseLevBufferPool& _pool;

    buffer* grow(buffer* a, int64_t t, int64_t b) noexcept
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

    // TSan doesn't model fences. Its builds use seq_cst and release accesses in their place.
    void store_bottom(int64_t b) noexcept
    {
        atomic_store(&_bottom, b, fm_TSAN ? memory_order::release : memory_order::relaxed);
    }

public:
    explicit ChaseLevDeque(ChaseLevBufferPool& pool, uint32_t log2_capacity = 8) noexcept :
        _array{(uintptr_t)pool.get(log2_capacity)},
        _pool{pool}
    {
    }

    ~ChaseLevDeque() noexcept
    {
        _pool.put((buffer*)_array);
    }

    fm_DISABLE_MOVE_COPY(ChaseLevDeque);

    void push(T* x) noexcept
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

    T* take() noexcept
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

    T* steal() noexcept
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
};

} // namespace floormat
