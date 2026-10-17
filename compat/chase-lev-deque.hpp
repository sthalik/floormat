#pragma once
#include "compat/assert.hpp"
#include "compat/atomic.hpp"
#include "compat/defs.hpp"
#include <new>

namespace floormat {

// One owner calls push() and take(), thief i calls steal(i). Both return nullptr when empty
// or on a lost race. Only the owner may push. Any other thread that makes jobs needs its own
// deque or a separate multi-producer queue.
template<typename T>
class ChaseLevDeque final
{
    struct alignas(64) buffer
    {
        int64_t mask;

        volatile uintptr_t& operator[](int64_t i) noexcept
        {
            return reinterpret_cast<volatile uintptr_t*>(this + 1)[i & mask];
        }

        static buffer* make(int64_t capacity) noexcept
        {
            void* p = ::operator new(sizeof(buffer) + (size_t)capacity * sizeof(uintptr_t), std::align_val_t{alignof(buffer)});
            return new (p) buffer{capacity - 1};
        }

        static void destroy(buffer* b) noexcept
        {
            ::operator delete(b, std::align_val_t{alignof(buffer)});
        }
    };

    struct alignas(64) hazard
    {
        volatile uintptr_t value = 0;
    };

    // A thief can hold a pointer to any array, so grow() keeps them until no hazard names
    // them. Arrays only double, so 64 entries never fill.
    static constexpr uint32_t max_retired = 64;

    alignas(64) volatile int64_t _top = 0;
    alignas(64) volatile int64_t _bottom = 0;
    volatile uintptr_t _array;
    alignas(64) hazard* const _hazards;
    const uint32_t _thief_count;
    uint32_t _retired_count = 0;
    buffer* _retired[max_retired];

    buffer* grow(buffer* a, int64_t t, int64_t b) noexcept
    {
        buffer* n = buffer::make(2 * (a->mask + 1));
        for (int64_t i = t; i < b; ++i)
            atomic_store(&(*n)[i], atomic_load(&(*a)[i], memory_order::relaxed), memory_order::relaxed);
        // seq_cst for the hazard check in reclaim(); the paper's relaxed store has no reclamation.
        atomic_store(&_array, (uintptr_t)n, memory_order::seq_cst);
        fm_assert(_retired_count < max_retired);
        _retired[_retired_count++] = a;
        return n;
    }

    bool is_hazard(buffer* a) const noexcept
    {
        for (uint32_t i = 0; i < _thief_count; ++i)
            if (atomic_load(&_hazards[i].value, memory_order::seq_cst) == (uintptr_t)a)
                return true;
        return false;
    }

    void reclaim() noexcept
    {
        uint32_t kept = 0;
        for (uint32_t i = 0; i < _retired_count; ++i)
            if (is_hazard(_retired[i]))
                _retired[kept++] = _retired[i];
            else
                buffer::destroy(_retired[i]);
        _retired_count = kept;
    }

    // TSan doesn't model fences. Its builds use seq_cst and release accesses in their place.
    void store_bottom(int64_t b) noexcept
    {
        atomic_store(&_bottom, b, fm_TSAN ? memory_order::release : memory_order::relaxed);
    }

public:
    explicit ChaseLevDeque(uint32_t thief_count, uint32_t log2_capacity = 8) noexcept :
        _array{(uintptr_t)buffer::make(int64_t{1} << log2_capacity)},
        _hazards{new hazard[thief_count]},
        _thief_count{thief_count}
    {
        fm_assert(log2_capacity < 62);
    }

    ~ChaseLevDeque() noexcept
    {
        for (uint32_t i = 0; i < _retired_count; ++i)
            buffer::destroy(_retired[i]);
        buffer::destroy((buffer*)_array);
        delete[] _hazards;
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
        if (_retired_count) [[unlikely]]
            reclaim();
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
        if (_retired_count) [[unlikely]]
            reclaim();
        return x;
    }

    T* steal(uint32_t thief) noexcept
    {
        fm_debug_assert(thief < _thief_count);
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

        volatile uintptr_t* h = &_hazards[thief].value;
        uintptr_t a = atomic_load(&_array, memory_order::acquire);
        for (;;)
        {
            atomic_exchange(h, a, memory_order::seq_cst);
            uintptr_t a2 = atomic_load(&_array, memory_order::seq_cst);
            if (a2 == a)
                break;
            a = a2;
        }
        // Read before the CAS: once top moves past t, push() may reuse the slot.
        auto* x = (T*)atomic_load(&(*(buffer*)a)[t], memory_order::relaxed);
        bool won = atomic_compare_exchange(&_top, t, t + 1, memory_order::seq_cst);
        atomic_store(h, uintptr_t{0}, memory_order::release);
        return won ? x : nullptr;
    }
};

} // namespace floormat
