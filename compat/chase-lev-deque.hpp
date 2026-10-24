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
    static void after_array_load() noexcept;
};

// Arrays are freed only by the destructor, so a thief holding a stale one reads valid memory.
class ChaseLevBufferPool final
{
public:
    struct alignas(64) buffer
    {
        int64_t mask;
        buffer* next;

        volatile uintptr_t& operator[](int64_t i) noexcept;
    };

    ChaseLevBufferPool() noexcept = default;

    // Every deque using the pool must be destroyed first.
    ~ChaseLevBufferPool() noexcept;

    fm_DISABLE_MOVE_COPY(ChaseLevBufferPool);

    buffer* get(uint32_t log2_capacity) noexcept;
    void put(buffer* b) noexcept;

private:
    static constexpr uint32_t max_log2_capacity = 62;

    static void push(volatile uintptr_t* head, buffer* first) noexcept;

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

    buffer* grow(buffer* a, int64_t t, int64_t b) noexcept;

    // TSan doesn't model fences. Its builds use seq_cst and release accesses in their place.
    void store_bottom(int64_t b) noexcept;

public:
    explicit ChaseLevDeque(ChaseLevBufferPool& pool, uint32_t log2_capacity = 8) noexcept;
    ~ChaseLevDeque() noexcept;

    fm_DISABLE_MOVE_COPY(ChaseLevDeque);

    void push(T* x) noexcept;
    T* take() noexcept;
    T* steal() noexcept;
};

} // namespace floormat
