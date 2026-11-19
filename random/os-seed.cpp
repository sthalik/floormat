#include "os-seed.inl"
#include "random.hpp"
#include <cstring>

namespace floormat::Random {

template void os_random_bytes<8>(u8 (&out)[8]);
template void os_random_bytes<32>(u8 (&out)[32]);

void seed_from_entropy(splitmix64& state)
{
    u8 bytes[sizeof state.x];
    os_random_bytes(bytes);
    std::memcpy(&state.x, bytes, sizeof bytes);
}

void seed_from_entropy(xoshiro256starstar& state)
{
    u8 bytes[sizeof state.s];
    do
    {
        os_random_bytes(bytes);
        std::memcpy(state.s, bytes, sizeof bytes);
    }
    while ((state.s[0] | state.s[1] | state.s[2] | state.s[3]) == 0);
}

void seed_from_entropy(ranlux48& state)
{
    u8 bytes[sizeof(u64)];
    os_random_bytes(bytes);
    u64 value;
    std::memcpy(&value, bytes, sizeof bytes);
    seed(state, value);
}

} // namespace floormat::Random
