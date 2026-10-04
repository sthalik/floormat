#pragma once
#include "atomic.hpp"
#include "defs.hpp"

namespace floormat {

class AtomicFlag final
{
    volatile uint8_t state = 0;

public:
    constexpr AtomicFlag() noexcept = default;
    fm_DISABLE_MOVE_COPY(AtomicFlag);

    // Returns the previous value.
    bool test_and_set(memory_order o = memory_order::seq_cst) noexcept;
    void clear(memory_order o = memory_order::seq_cst) noexcept;
    [[nodiscard]] bool test(memory_order o = memory_order::seq_cst) const noexcept;
};

} // namespace floormat
