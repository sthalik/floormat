#if 1
#include "entropy/xoroshiro.hpp"
#include "entropy/murmur.hpp"
#include <random>
#include <cstdio>
#include <cinttypes>

void gen()
{
#if 0
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
    static constexpr char const foo[] = "bleh bleh";
    static constexpr char const bar[] = "DUPA";

    auto [a, b] = murmur128(foo, sizeof(foo) - 1);
    printf("%" PRIx64 "%" PRIx64 "\n", a, b);

    auto [c, d] = murmur128(bar, sizeof(bar) - 1);
    printf("%" PRIx64 "%" PRIx64 "\n", c, d);
    fflush(stdout);
#endif
}

#endif
