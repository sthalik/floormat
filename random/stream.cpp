#include "random.hpp"
#include "chacha.hpp"
#include "os-seed.hpp"
#include <charconv>
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

constexpr u32 buffer_size = 3 << 15;
unsigned char buffer[buffer_size]; // todo thread safety

// Packs the low Bytes of each output back to back, so a 48-bit generator has no constant byte.
template<u32 Bytes, typename Next>
void write(Next&& next)
{
    static_assert(buffer_size % Bytes == 0);
    for (;;)
    {
        for (u32 i = 0; i < buffer_size; i += Bytes)
        {
            const u64 x = next();
            std::memcpy(buffer + i, &x, Bytes);
        }
        if (std::fwrite(buffer, 1, buffer_size, stdout) != buffer_size)
            return;
    }
}

int usage()
{
    std::fputs("usage: floormat-random-stream "
               "xoshiro256starstar|splitmix64|mix-counter|ranlux48|gaussian|chacha8 [seed]\n"
               "Writes raw little-endian outputs to stdout until the reader closes it.\n"
               "gaussian writes the normal CDF of each deviate as 32 bits.\n"
               "chacha8 reads its key as 64 hex digits from seed, else from the OS, and prints it to stderr.\n",
               stderr);
    return EXIT_FAILURE;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
        return usage();
    const char* name = argv[1];
    const u64 seed = argc == 3 ? std::strtoull(argv[2], nullptr, 0) : 0;
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
        u64 i = seed;
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
            return (u64)std::fmin(p * 0x1p32, 0xffffffff);
        });
    }
    else if (!std::strcmp(name, "chacha8"))
    {
        u8 key[32];
        if (argc == 3)
        {
            const char* hex = argv[2];
            if (std::strlen(hex) != 2 * sizeof key)
                return usage();
            for (u32 i = 0; i < sizeof key; i++)
            {
                const auto [ptr, ec] = std::from_chars(hex + 2*i, hex + 2*i + 2, key[i], 16);
                if (ec != std::errc{} || ptr != hex + 2*i + 2)
                    return usage();
            }
        }
        else
            Random::os_random_bytes(key);
        std::fputs("chacha8 key ", stderr);
        for (u8 byte : key)
            std::fprintf(stderr, "%02x", byte);
        std::fputc('\n', stderr);
        Random::ChaCha::csprng g{key};
        write<8>([&] { return g.next(); });
    }
    else
        return usage();
    return EXIT_SUCCESS;
}
