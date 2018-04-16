#if 1
#include "xoroshiro.hpp"
#include "murmur.hpp"
#include <random>
#include <cstdio>

volatile union {
    std::uint32_t u32;
    std::uint8_t u8[4];
} tmp;
static_assert(sizeof(tmp) == 4, "");

void gen()
{
#if 1
    std::independent_bits_engine<xoroshiro, 8, unsigned> d;
    for (unsigned i = 0; i < 10; i++)
    {
        for (unsigned k = 0; k < 50; k++)
        {
            printf("%02x ", d());
        }
        printf("\n");
    }
#else
    const char* foo = "bleh bleh";
    const char* bar = "DUPA";
    using t = unsigned long long;
    std::uint64_t a, b;

    std::tie(a, b) = murmur128(foo, sizeof(foo) - 1);
    printf("%llu, %llu\n", t(a), t(b));

    std::tie(a, b) = murmur128(bar, sizeof(bar) - 1);
    printf("%llu, %llu\n", t(a), t(b));
    fflush(stdout);
#endif
}

#endif
