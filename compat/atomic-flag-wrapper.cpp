#include "atomic-flag-wrapper.hpp"

#if defined __GNUC__ && !defined __clang__
// Same -Wattributes case as src/spritebatch.cpp.
#pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace floormat {

[[fm_always_inline]] bool AtomicFlag::test_and_set(memory_order o) noexcept { return atomic_exchange(&state, 1, o) != 0; }
[[fm_always_inline]] void AtomicFlag::clear(memory_order o) noexcept { atomic_store(&state, 0, o); }
[[fm_always_inline]] bool AtomicFlag::test(memory_order o) const noexcept { return atomic_load(&state, o) != 0; }

} // namespace floormat
