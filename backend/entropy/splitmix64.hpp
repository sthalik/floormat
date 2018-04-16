#pragma once

#include <cstdint>

struct splitmix64 final
{
    std::uint64_t seed;

    uint64_t operator()();

    splitmix64(std::uint64_t seed) : seed(seed) {}
};
