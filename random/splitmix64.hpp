#pragma once
#include <random>

namespace floormat {

struct splitmix64
{
    using result_type = uint64_t;

    uint64_t x;

    static constexpr uint64_t min() noexcept { return 0; }
    static constexpr uint64_t max() noexcept { return (uint64_t)-1; }

    constexpr uint64_t operator()() noexcept
    {
        uint64_t z = (x += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        return z ^ (z >> 31);
    }
};

static_assert(std::uniform_random_bit_generator<splitmix64>);

} // namespace floormat
