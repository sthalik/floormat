#pragma once

namespace floormat::Random {

// Fills out from the OS's cryptographically secure generator. Aborts if that fails.
template<u32 N> void os_random_bytes(u8 (&out)[N]);

extern template void os_random_bytes<8>(u8 (&out)[8]);
extern template void os_random_bytes<32>(u8 (&out)[32]);

} // namespace floormat::Random
