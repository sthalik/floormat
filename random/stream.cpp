#include "random/random.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using namespace floormat;

namespace {

constexpr uint32_t buffer_size = 3 << 15;
unsigned char buffer[buffer_size];

// Packs the low Bytes of each output back to back, so a 48-bit generator has no constant byte.
template<uint32_t Bytes, typename Next>
void write(Next&& next)
{
    static_assert(buffer_size % Bytes == 0);
    for (;;)
    {
        for (uint32_t i = 0; i < buffer_size; i += Bytes)
        {
            const uint64_t x = next();
            std::memcpy(buffer + i, &x, Bytes);
        }
        if (std::fwrite(buffer, 1, buffer_size, stdout) != buffer_size)
            return;
    }
}

int usage()
{
    std::fputs("usage: floormat-random-stream xoshiro256starstar|splitmix64|mix-counter|ranlux48|gaussian [seed]\n"
               "Writes raw little-endian outputs to stdout until the reader closes it.\n"
               "gaussian writes the normal CDF of each deviate as 32 bits.\n", stderr);
    return EXIT_FAILURE;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
        return usage();
    const char* name = argv[1];
    const uint64_t seed = argc == 3 ? std::strtoull(argv[2], nullptr, 0) : 0;
#ifdef _WIN32
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif

    if (!std::strcmp(name, "xoshiro256starstar"))
    {
        Random::xoshiro256starstar g;
        Random::seed(g, seed);
        write<8>([&] { return Random::next(g); });
    }
    else if (!std::strcmp(name, "splitmix64"))
    {
        Random::splitmix64 g;
        Random::seed(g, seed);
        write<8>([&] { return Random::next(g); });
    }
    else if (!std::strcmp(name, "mix-counter"))
    {
        uint64_t i = seed;
        write<8>([&] { return Random::mix(i++); });
    }
    else if (!std::strcmp(name, "ranlux48"))
    {
        Random::ranlux48 g;
        Random::seed(g, seed);
        write<6>([&] { return Random::next(g); });
    }
    else if (!std::strcmp(name, "gaussian"))
    {
        Random::xoshiro256starstar g;
        Random::seed(g, seed);
        write<4>([&] {
            const double p = 0.5 * std::erfc(-Random::gaussian(Random::next(g)) * 0.70710678118654752);
            return (uint64_t)std::fmin(p * 0x1p32, 0xffffffff);
        });
    }
    else
        return usage();
    return EXIT_SUCCESS;
}
