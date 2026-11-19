#pragma once

namespace floormat::Random::ChaCha {

constexpr inline u32 rounds = 8;

class csprng
{
    alignas(16) u64 buffer[256];
    u32 seed[8];
    u64 stream, counter;
    u32 index;

    void refill();

public:
    csprng();
    explicit csprng(const u8 (&seed)[32], u64 stream = 0);

    u64 next();
    void fill(u64* out, u32 n);
    void discard(u64 n);
};

} // namespace floormat::Random::ChaCha
