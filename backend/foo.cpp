#if 1
#include "xorshift.hpp"
#include "murmur.hpp"
#include <random>
#include <cstdio>

void gen()
{
#if 1
    xorshift gen;
    std::uniform_int_distribution<std::int32_t> d;
    volatile union {
        std::uint32_t u32;
        std::uint8_t u8[4];
    } tmp; static_assert(sizeof(tmp) == 4, "");
    for (unsigned i = 0; i < 100 * 1024 * 1024; i++)
    {
        //printf("%c", d(gen));
        tmp.u32 = d(gen);
    }
    (void)tmp;
#else
    const char* foo = "bleh bleh";
    const char* bar = "DUPA";
    using t = unsigned long long;
    std::uint64_t a, b;

    std::tie(a, b) = murmur128(foo, sizeof(foo) - 1);
    printf("%llu, %llu\n", t(a), t(b));

    std::tie(a, b) = murmur128(bar, sizeof(bar) - 1);
    printf("%llu, %llu\n", t(a), t(b));
#endif
}

#endif
