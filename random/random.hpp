#pragma once

namespace floormat::Random {

// States are plain data. A savegame can store one verbatim to continue its sequence.
// A default-constructed state equals seed(state, 0).
struct splitmix64 { u64 x = 0; };
// All zero is a fixed point that outputs 0 forever. seed() never produces it.
struct xoshiro256starstar
{
    u64 s[4] = { 0xe220a8397b1dcdaf, 0x6e789e6aa1b965f4, 0x06c45d188009454f, 0xf88bb8a8724c81ec };
};
// std::ranlux48's sequence. Its state layout differs between standard libraries.
struct ranlux48
{
    u64 x[12] = {
        0x09e548e6f632, 0x73325a23cd04, 0x1e9928ecc09a, 0x473530f584e4,
        0xa32e3cd32e48, 0x580615b0b74b, 0xd8a434591dbc, 0x1f3b45cc715e,
        0x8d3e66fdbfe3, 0x1a6371edca94, 0xbe536a408fec, 0xefe778571604,
    };
    u8 i = 0, c = 0, n = 0;
};

// mix(0) == 0.
u64 mix(u64 x);

void seed(splitmix64& state, u64 value);
void seed(xoshiro256starstar& state, u64 value);
// Zero selects the standard's default seed.
void seed(ranlux48& state, u64 value);

u64 next(splitmix64& state);
u64 next(xoshiro256starstar& state);
// 48-bit outputs.
u64 next(ranlux48& state);

// Standard normal quantile of (u + 1/2) / 2^64, the same on every compiler.
// gaussian(~u) == -gaussian(u).
template<typename T = double> T gaussian(u64 u) = delete;
template<> float gaussian<float>(u64 u);
template<> double gaussian<double>(u64 u);
// center + spread * gaussian(u), computed in double.
template<typename T> T gaussian(u64 u, T center, T spread) = delete;
template<> float gaussian<float>(u64 u, float center, float spread);
template<> double gaussian<double>(u64 u, double center, double spread);

} // namespace floormat::Random
