#include "compat/atomic.hpp"
#include <atomic>

namespace floormat {

static_assert((int)memory_order::relaxed == (int)std::memory_order_relaxed);
static_assert((int)memory_order::acquire == (int)std::memory_order_acquire);
static_assert((int)memory_order::release == (int)std::memory_order_release);
static_assert((int)memory_order::acq_rel == (int)std::memory_order_acq_rel);
static_assert((int)memory_order::seq_cst == (int)std::memory_order_seq_cst);

} // namespace floormat
