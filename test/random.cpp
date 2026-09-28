#include "app.hpp"
#include "random/random.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <random>

namespace floormat::Test {

namespace {

struct reference_values
{
    uint64_t seed;
    uint64_t state[4];
    uint64_t outputs[8];
    uint64_t output_1000000;
};

// From the C reference at prng.di.unimi.it, with s[] filled by four splitmix64 outputs.
constexpr inline reference_values references[] = {
    {
        0x0000000000000000,
        { 0xe220a8397b1dcdaf, 0x6e789e6aa1b965f4, 0x06c45d188009454f, 0xf88bb8a8724c81ec },
        { 0x99ec5f36cb75f2b4, 0xbf6e1f784956452a, 0x1a5f849d4933e6e0, 0x6aa594f1262d2d2c,
          0xbba5ad4a1f842e59, 0xffef8375d9ebcaca, 0x6c160deed2f54c98, 0x8920ad648fc30a3f },
        0xec96d2d5eae0cff7,
    },
    {
        0x0000000000000001,
        { 0x910a2dec89025cc1, 0xbeeb8da1658eec67, 0xf893a2eefb32555e, 0x71c18690ee42c90b },
        { 0xb3f2af6d0fc710c5, 0x853b559647364cea, 0x92f89756082a4514, 0x642e1c7bc266a3a7,
          0xb27a48e29a233673, 0x24c123126ffda722, 0x123004ef8df510e6, 0x61954dcc47b1e89d },
        0xe1a406c2f015028f,
    },
    {
        0x9e3779b97f4a7c15,
        { 0x6e789e6aa1b965f4, 0x06c45d188009454f, 0xf88bb8a8724c81ec, 0x1b39896a51a8749b },
        { 0x422ea740d0977210, 0xe062b061b42e2928, 0x5a071fc5930841b6, 0x01334ef8ed3cc2bd,
          0xe45cbd6a2d9e96db, 0x3bc1fe841a5f292f, 0x60001d95ebbbd8e6, 0xa0aee00b5b303762 },
        0x3f68108d50daba58,
    },
    {
        0xffffffffffffffff,
        { 0xe4d971771b652c20, 0xe99ff867dbf682c9, 0x382ff84cb27281e9, 0x6d1db36ccba982d2 },
        { 0x8f5520d52a7ead08, 0xc476a018caa1802d, 0x81de31c0d260469e, 0xbf658d7e065f3c2f,
          0x913593fda1bca32a, 0xbb535e93941ba525, 0x5ecda415c3c6dfde, 0xc487398fc9de9ae2 },
        0x2bd0e2e1a8e68e57,
    },
};

bool same_state(const Random::xoshiro256starstar& a, const Random::xoshiro256starstar& b)
{
    return a.s[0] == b.s[0] && a.s[1] == b.s[1] && a.s[2] == b.s[2] && a.s[3] == b.s[3];
}

// Same algorithm and return value as brent() in random/period-test.cpp.
template<typename F>
uint32_t brent(Random::xoshiro256starstar hare, uint32_t max_steps, F&& on_output)
{
    auto tortoise = hare;
    on_output(Random::next(hare));
    uint32_t power = 1, lambda = 1, steps = 1;
    while (!same_state(tortoise, hare))
    {
        if (steps == max_steps)
            return 0;
        if (power == lambda)
        {
            tortoise = hare;
            power *= 2;
            lambda = 0;
        }
        on_output(Random::next(hare));
        lambda++;
        steps++;
    }
    return lambda;
}

void test_zero_state()
{
    // The first splitmix64 counter value is 0 and mix(0) == 0.
    Random::xoshiro256starstar g0;
    Random::seed(g0, 0 - 0x9e3779b97f4a7c15);
    fm_assert(g0.s[0] == 0);
    fm_assert(g0.s[1] != 0 && g0.s[2] != 0 && g0.s[3] != 0);
    auto g = g0;
    for (uint32_t i = 0; i < 64; i++)
        fm_assert(Random::next(g) != 0);
}

void test_default_states()
{
    Random::splitmix64 a;
    Random::xoshiro256starstar b;
    Random::ranlux48 c;
    for (uint32_t i = 0; i < 1000; i++)
    {
        (void)Random::next(a);
        (void)Random::next(b);
        (void)Random::next(c);
    }
    Random::seed(a, 0);
    Random::seed(b, 0);
    Random::seed(c, 0);
    fm_assert(a.x == Random::splitmix64{}.x);
    fm_assert(same_state(b, Random::xoshiro256starstar{}));
    const Random::ranlux48 d;
    fm_assert(c.i == d.i && c.c == d.c && c.n == d.n);
    for (uint32_t i = 0; i < 12; i++)
        fm_assert(c.x[i] == d.x[i]);
}

void test_mix()
{
    constexpr uint64_t pairs[][2] = {
        { 0x0000000000000000, 0x0000000000000000 },
        { 0x0000000000000001, 0x071894de00d9981f },
        { 0x0000000000000002, 0xef9d98262a1b46cb },
        { 0x9e3779b97f4a7c15, 0xd66f0f541be4e401 },
        { 0x8000000000000000, 0xe0a78385dbb4eed5 },
        { 0xffffffffffffffff, 0x96c7cbb7179e89f6 },
    };
    for (const auto& [x, y] : pairs)
        fm_assert(Random::mix(x) == y);

    constexpr uint32_t N = 1 << 12;
    uint32_t flips[64][64] = {};
    Random::xoshiro256starstar g;
    Random::seed(g, 0x5eed);
    for (uint32_t k = 0; k < N; k++)
    {
        const uint64_t x = Random::next(g), y = Random::mix(x);
        for (uint32_t i = 0; i < 64; i++)
        {
            const uint64_t d = y ^ Random::mix(x ^ (uint64_t{1} << i));
            for (uint32_t j = 0; j < 64; j++)
                flips[i][j] += d >> j & 1;
        }
    }
    // Each count is binomial(N, 1/2) with standard deviation 32. Allow 8 of them.
    for (const auto& row : flips)
        for (uint32_t count : row)
            fm_assert(count > N/2 - N/16 && count < N/2 + N/16);
}

void test_ranlux48()
{
    {
        Random::ranlux48 g;
        uint64_t x = 0;
        for (uint32_t i = 0; i < 10000; i++)
            x = Random::next(g);
        fm_assert(x == 249142670248501); // [rand.predef]
    }

    constexpr uint64_t seeds[] = {
        0, 1, 19780503, 2147483563, 2147483564, 0x9e3779b97f4a7c15, 0xffffffffffffffff,
    };
    for (uint64_t seed : seeds)
    {
        Random::ranlux48 g;
        Random::seed(g, seed);
        std::ranlux48 h{seed};
        for (uint32_t i = 0; i < 2000; i++)
            fm_assert(Random::next(g) == h());
    }
}

template<typename T>
void test_gaussian(uint64_t expected_hash)
{
    const auto cdf = [](double z) { return 0.5 * std::erfc(-z * 0.70710678118654752); };
    // Rounding z to T moves cdf(z) by at most a relative (z^2 + 1) half_ulp, from the Mills ratio.
    constexpr double half_ulp = std::numeric_limits<T>::epsilon() / 2;
    const auto tolerance = [](double z) { return 1e-13 + (z*z + 1) * half_ulp; };
    uint64_t hash = 0;

    // Powers of two reach both tail formulas. Random inputs almost never do.
    for (uint32_t k = 0; k < 64; k++)
    {
        const uint64_t x = uint64_t{1} << k;
        for (uint64_t u : { x - 1, x, x + x/3 })
        {
            const double z = Random::gaussian<T>(u), p = ((double)u + 0.5) * 0x1p-64;
            fm_assert(std::fabs(cdf(z) - p) <= tolerance(z) * p);
            fm_assert((double)Random::gaussian<T>(~u) == -z);
            hash = Random::mix(hash ^ std::bit_cast<uint64_t>(z));
        }
    }

    double last = -10;
    for (uint32_t k = 0; k < 1 << 16; k++)
    {
        const uint64_t u = 0x0123456789ab + (uint64_t{k} << 48);
        const double z = Random::gaussian<T>(u), p = ((double)u + 0.5) * 0x1p-64;
        fm_assert(z > last);
        if (p < 0.5)
            fm_assert(std::fabs(cdf(z) - p) <= tolerance(z) * p);
        last = z;
    }

    // For |q| < 2^-30, z = sqrt(2 pi) q within a relative 2^-60.
    for (uint32_t k = 0; k < 34; k++)
    {
        const uint64_t n = uint64_t{1} << k, u = (uint64_t{1} << 63) - 1 - n;
        const double q = -((double)n + 0.5) * 0x1p-64;
        fm_assert(std::fabs((double)Random::gaussian<T>(u) / (2.5066282746310002 * q) - 1) < 1e-15 + half_ulp);
    }
    last = -1;
    for (uint64_t u = (uint64_t{1} << 63) - 1000; u != (uint64_t{1} << 63) + 1000; u++)
    {
        const double z = Random::gaussian<T>(u);
        fm_assert(z > last);
        fm_assert((z < 0) == (u < (uint64_t{1} << 63)));
        last = z;
    }

    constexpr uint32_t N = 1 << 18;
    double sum[4] = {};
    Random::xoshiro256starstar g;
    Random::seed(g, 1);
    for (uint32_t i = 0; i < N; i++)
    {
        const double z = Random::gaussian<T>(Random::next(g));
        sum[0] += z;
        sum[1] += z*z;
        sum[2] += z*z*z;
        sum[3] += z*z*z*z;
        hash = Random::mix(hash ^ std::bit_cast<uint64_t>(z));
    }
    // Raw moments 0, 1, 0, 3. Limits are 5 standard errors: sqrt(1/N), sqrt(2/N), sqrt(15/N), sqrt(96/N).
    fm_assert(std::fabs(sum[0] / N) < 0.0098);
    fm_assert(std::fabs(sum[1] / N - 1) < 0.0138);
    fm_assert(std::fabs(sum[2] / N) < 0.0378);
    fm_assert(std::fabs(sum[3] / N - 3) < 0.0957);

    fm_assert(hash == expected_hash);
}

struct quantile { uint64_t u; double z; };

// Phi^-1((u + 1/2) / 2^64) from mpmath, rounded to double.
// Rounding these to float gives the same values as rounding the exact quantile.
constexpr inline quantile quantiles[] = {
    { 0x0000000000000000, -0x1.24f82aa55eb14p+3 },
    { 0x0000000000000001, -0x1.21263f48fda58p+3 },
    { 0x0000000100000000, -0x1.8ebc95048a0d4p+2 },
    { 0x0004000000000000, -0x1.ebc4627bdd628p+1 },
    { 0x1000000000000000, -0x1.88bc1fbe1dabep+0 },
    { 0x4000000000000000, -0x1.5956b87528a49p-1 },
    { 0x7ffffffffffffff0, -0x1.36d2686f675cdp-59 },
    { 0x7fffffffffffffff, -0x1.40d931ff62706p-64 },
    { 0x000000043cc3bc02, -0x1.7fffffffff7edp+2 },
    { 0x00003900e51ba760, -0x1.2000000000000p+2 },
    { 0x00587787e616d75a, -0x1.8000000000000p+1 },
    { 0x00587787e616d75b, -0x1.8000000000000p+1 },
    { 0x00587787e616d75c, -0x1.8000000000000p+1 },
    { 0x00c34821a4f64016, -0x1.6000000000000p+1 },
    { 0x05d2f3e0b27617e6, -0x1.0000000000000p+1 },
    { 0x289da176f964165b, -0x1.0000000000000p+0 },
    { 0x66bb2ea748949e57, -0x1.0000000000000p-2 },
    { 0x123456789abcdef0, -0x1.77b26344b88f2p+0 },
    { 0x5555555555555555, -0x1.b91093bfdfa2ap-2 },
    { 0xa5a5a5a5a5a5a5a5, 0x1.827308a211ed9p-2 },
    { 0x8000000000000000, 0x1.40d931ff62706p-64 },
    { 0xc000000000000000, 0x1.5956b87528a49p-1 },
    { 0xffffffffffffffff, 0x1.24f82aa55eb14p+3 },
};

void test_gaussian_types()
{
    // center + spread * z cancels to 0 at the -2.75 reference.
    constexpr double center = 3.4375, spread = 1.25;
    for (const auto& [u, ref] : quantiles)
    {
        const double z = Random::gaussian<double>(u);
        fm_assert(std::fabs(z - ref) <= 2e-15 * std::fabs(z));
        fm_assert(Random::gaussian<float>(u) == (float)ref);
        fm_assert(Random::gaussian<float>(~u) == -(float)ref);

        fm_assert(Random::gaussian(u, 0.0, 1.0) == z);
        fm_assert(Random::gaussian(u, 0.f, 1.f) == Random::gaussian<float>(u));
        const double y = center + spread * ref, error = 2e-15 * (center + spread * std::fabs(ref));
        fm_assert(std::fabs(Random::gaussian(u, center, spread) - y) <= 0x1p-53 * std::fabs(y) + error);
        fm_assert(std::fabs((double)Random::gaussian(u, (float)center, (float)spread) - y) <= 0x1p-24 * std::fabs(y) + error);
    }
}

} // namespace

void test_random()
{
    test_zero_state();
    test_default_states();
    test_mix();
    test_ranlux48();
    test_gaussian<double>(0x7fcb08b40c66e350);
    test_gaussian<float>(0x9c8265a5f7ca8afe);
    test_gaussian_types();

    constexpr uint32_t max_steps = 1 << 20;

    {
        Random::xoshiro256starstar g{{1, 2, 3, 4}};
        fm_assert(Random::next(g) == 11520);
        fm_assert(Random::next(g) == 0);
    }

    for (const auto& r : references)
    {
        Random::xoshiro256starstar g;
        Random::seed(g, r.seed);
        fm_assert((g.s[0] | g.s[1] | g.s[2] | g.s[3]) != 0);
        for (uint32_t i = 0; i < 4; i++)
            fm_assert(g.s[i] == r.state[i]);

        uint32_t n = 0;
        uint64_t output_1000000 = 0;
        const uint32_t lambda = brent(g, max_steps, [&](uint64_t x) {
            if (n < 8)
                fm_assert(x == r.outputs[n]);
            if (++n == 1000000)
                output_1000000 = x;
        });
        fm_assert(lambda == 0);
        fm_assert(n == max_steps);
        fm_assert(output_1000000 == r.output_1000000);
    }
}

} // namespace floormat::Test
