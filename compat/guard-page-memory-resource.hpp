#pragma once
#include "defs.hpp"
#include "integer-types.hpp"

namespace floormat {

// Maps each buffer at a random address. Each buffer ends less than its alignment before its pages' end.
// allocate() and deallocate() may run on any thread at once.
class guard_page_memory_resource final
{
    u32 _num_allocated_buffers = 0;

public:
    static constexpr usize max_alignment = 8192;

    constexpr guard_page_memory_resource() noexcept = default;
    // Every buffer must be deallocated before destruction.
    ~guard_page_memory_resource() noexcept;
    fm_DISABLE_MOVE_COPY(guard_page_memory_resource);

    // alignment must be a power of two, at most max_alignment.
    [[nodiscard]] void* allocate(usize bytes, usize alignment) noexcept;
    void deallocate(void* p, usize bytes, usize alignment) noexcept;
    u32 num_allocated_buffers() const noexcept;
};

} // namespace floormat
