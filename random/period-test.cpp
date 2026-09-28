#include "random/random.hpp"
#include <bit>
#include <chrono>
#include <cinttypes>
#include <cstdio>

namespace floormat {

namespace {

using clock_type = std::chrono::steady_clock;
using Random::splitmix64;
using Random::xoshiro256starstar;

constexpr inline uint32_t phase_a_seeds = 1024;
constexpr inline uint32_t phase_a_log2_steps = 28;
constexpr inline uint64_t phase_b_seed = (uint64_t)-1;
constexpr inline uint32_t phase_b_log2_steps = 36;
constexpr inline uint32_t splitmix_log2_steps = 32;
constexpr inline uint32_t masked_bits = 20;
constexpr inline uint32_t stuck_bit_index = 37;

constexpr inline uint64_t run_limit = 2;
constexpr inline uint64_t zero_limit = 2;
constexpr inline uint32_t coverage_log2_outputs = 16;
constexpr inline uint32_t balance_log2_outputs = 24;
constexpr inline uint64_t balance_mean = uint64_t{1} << (balance_log2_outputs - 1);
// 6 standard deviations of a bit count, sqrt(2^24 / 4) = 2048.
constexpr inline uint64_t balance_tolerance = 6 * 2048;

double seconds_since(clock_type::time_point start)
{
    return std::chrono::duration<double>(clock_type::now() - start).count();
}

const char* verdict(bool pass)
{
    return pass ? "PASS" : "FAIL";
}

// Returns the cycle length, or 0 if none shows up within max_steps steps.
// Finds cycles the start state leads into but isn't part of.
template<typename State, typename Step, typename Equal>
uint64_t brent(State hare, uint64_t max_steps, Step&& step, Equal&& equal)
{
    State tortoise = hare;
    step(hare);
    uint64_t power = 1, lambda = 1, steps = 1;
    while (!equal(tortoise, hare))
    {
        if (steps == max_steps)
            return 0;
        if (power == lambda)
        {
            tortoise = hare;
            power *= 2;
            lambda = 0;
        }
        step(hare);
        lambda++;
        steps++;
    }
    return lambda;
}

bool same_state(const xoshiro256starstar& a, const xoshiro256starstar& b)
{
    return a.s[0] == b.s[0] && a.s[1] == b.s[1] && a.s[2] == b.s[2] && a.s[3] == b.s[3];
}

struct output_stats
{
    uint64_t zeros = 0, prev = 0, run = 0, max_run = 0;

    void add(uint64_t x)
    {
        zeros += x == 0;
        run = x == prev ? run + 1 : 1;
        prev = x;
        if (run > max_run)
            max_run = run;
    }
};

template<typename Next>
uint64_t bits_taking_both_values(xoshiro256starstar g, uint64_t count, Next&& next)
{
    uint64_t any = 0, all = (uint64_t)-1;
    for (uint64_t i = 0; i < count; i++)
    {
        const uint64_t x = next(g);
        any |= x;
        all &= x;
    }
    return any & ~all;
}

struct bit_count_range
{
    uint64_t min, max;
};

template<typename Next>
bit_count_range count_bits(xoshiro256starstar g, uint64_t count, Next&& next)
{
    uint64_t counts[64] = {};
    for (uint64_t i = 0; i < count; i++)
    {
        const uint64_t x = next(g);
        for (uint32_t b = 0; b < 64; b++)
            counts[b] += x >> b & 1;
    }
    bit_count_range r{(uint64_t)-1, 0};
    for (uint64_t c : counts)
    {
        if (c < r.min)
            r.min = c;
        if (c > r.max)
            r.max = c;
    }
    return r;
}

struct seed_result
{
    uint64_t lambda;
    output_stats stats;
    uint64_t both_values;
};

template<typename Next>
seed_result test_seed(const xoshiro256starstar& start, uint64_t steps, Next&& next)
{
    output_stats stats;
    const uint64_t lambda = brent(start, steps, [&](xoshiro256starstar& g) { stats.add(next(g)); }, same_state);
    return { lambda, stats, bits_taking_both_values(start, uint64_t{1} << coverage_log2_outputs, next) };
}

bool runs_ok(const output_stats& stats)
{
    return stats.max_run <= run_limit;
}

bool coverage_ok(uint64_t both_values)
{
    return both_values == (uint64_t)-1;
}

bool balance_ok(bit_count_range r)
{
    return r.min >= balance_mean - balance_tolerance && r.max <= balance_mean + balance_tolerance;
}

bool seed_ok(const seed_result& r)
{
    return r.lambda == 0 && runs_ok(r.stats) && r.stats.zeros <= zero_limit && coverage_ok(r.both_values);
}

uint64_t plain_output(xoshiro256starstar& g)
{
    return Random::next(g);
}

// Advances the state so that brent() doesn't stop after the first output.
uint64_t constant_output(xoshiro256starstar& g)
{
    (void)Random::next(g);
    return 0x0123456789abcdef;
}

uint64_t stuck_bit_output(xoshiro256starstar& g)
{
    return Random::next(g) & ~(uint64_t{1} << stuck_bit_index);
}

bool check_detectors()
{
    bool ok = true;

    {
        constexpr uint64_t mask = (uint64_t{1} << masked_bits) - 1;
        const auto start = clock_type::now();
        // brent() finds a cycle of length 2^k after 2^(k+1) - 1 steps.
        const uint64_t lambda = brent(splitmix64{0}, uint64_t{1} << (masked_bits + 1),
                                      [](splitmix64& g) { (void)Random::next(g); g.x &= mask; },
                                      [](const splitmix64& a, const splitmix64& b) { return a.x == b.x; });
        const bool pass = lambda == uint64_t{1} << masked_bits;
        std::printf("detector check: splitmix64 with its state masked to %u bits\n", masked_bits);
        std::printf("  lambda: %" PRIu64 " (expected 2^%u)\n", lambda, masked_bits);
        std::printf("  elapsed: %.2f s\n", seconds_since(start));
        std::printf("  %s\n", verdict(pass));
        ok &= pass;
    }

    {
        const auto r = test_seed(xoshiro256starstar{}, uint64_t{1} << coverage_log2_outputs, constant_output);
        const bool pass = !runs_ok(r.stats);
        std::printf("detector check: constant output, the run check must fail\n");
        std::printf("  max run of identical outputs: %" PRIu64 " (limit %" PRIu64 ")\n", r.stats.max_run, run_limit);
        std::printf("  %s\n", verdict(pass));
        ok &= pass;
    }

    {
        const auto start = clock_type::now();
        const uint64_t both = bits_taking_both_values(xoshiro256starstar{}, uint64_t{1} << coverage_log2_outputs,
                                                      stuck_bit_output);
        const auto counts = count_bits(xoshiro256starstar{}, uint64_t{1} << balance_log2_outputs, stuck_bit_output);
        const bool pass = !coverage_ok(both) && !balance_ok(counts);
        std::printf("detector check: bit %u stuck at 0, both bit checks must fail\n", stuck_bit_index);
        std::printf("  bits taking both values in the first 2^%u outputs: %d of 64\n",
                    coverage_log2_outputs, std::popcount(both));
        std::printf("  bit counts in the first 2^%u outputs: min %" PRIu64 ", max %" PRIu64 "\n",
                    balance_log2_outputs, counts.min, counts.max);
        std::printf("  elapsed: %.2f s\n", seconds_since(start));
        std::printf("  %s\n", verdict(pass));
        ok &= pass;
    }

    std::fflush(stdout);
    return ok;
}

void print_stats(uint64_t max_run, uint64_t zeros)
{
    std::printf("  max run of identical outputs: %" PRIu64 " (limit %" PRIu64 ")\n", max_run, run_limit);
    std::printf("  zero outputs: %" PRIu64 " (limit %" PRIu64 ")\n", zeros, zero_limit);
}

bool phase_a()
{
    const auto start = clock_type::now();
    const uint64_t steps = uint64_t{1} << phase_a_log2_steps;
    std::printf("phase A: seeds 0..%u, 2^%u steps each\n", phase_a_seeds - 1, phase_a_log2_steps);
    std::fflush(stdout);

    uint64_t max_run = 0, zeros = 0;
    uint32_t cycles = 0, failed_seeds = 0;
    int min_both = 64;
    for (uint32_t seed = 0; seed < phase_a_seeds; seed++)
    {
        xoshiro256starstar g;
        Random::seed(g, seed);
        const auto r = test_seed(g, steps, plain_output);
        cycles += r.lambda != 0;
        zeros += r.stats.zeros;
        if (r.stats.max_run > max_run)
            max_run = r.stats.max_run;
        if (std::popcount(r.both_values) < min_both)
            min_both = std::popcount(r.both_values);
        if (!seed_ok(r))
        {
            failed_seeds++;
            std::printf("  FAIL seed %u: lambda %" PRIu64 ", max run %" PRIu64 ", zero outputs %" PRIu64
                        ", bits taking both values %d\n",
                        seed, r.lambda, r.stats.max_run, r.stats.zeros, std::popcount(r.both_values));
            std::fflush(stdout);
        }
    }

    const bool pass = failed_seeds == 0 && zeros <= zero_limit;
    std::printf("  seeds with a cycle: %u\n", cycles);
    print_stats(max_run, zeros);
    std::printf("  bits taking both values in the first 2^%u outputs: at least %d of 64 in every seed\n",
                coverage_log2_outputs, min_both);
    std::printf("  elapsed: %.1f s\n", seconds_since(start));
    std::printf("  %s\n", verdict(pass));
    std::fflush(stdout);
    return pass;
}

bool phase_b()
{
    const auto start = clock_type::now();
    std::printf("phase B: seed 0x%016" PRIx64 ", 2^%u steps\n", phase_b_seed, phase_b_log2_steps);
    std::fflush(stdout);

    xoshiro256starstar g;
    Random::seed(g, phase_b_seed);
    const auto r = test_seed(g, uint64_t{1} << phase_b_log2_steps, plain_output);
    const auto counts = count_bits(g, uint64_t{1} << balance_log2_outputs, plain_output);
    const bool pass = seed_ok(r) && balance_ok(counts);

    if (r.lambda != 0)
        std::printf("  lambda: %" PRIu64 "\n", r.lambda);
    else
        std::printf("  lambda: no cycle\n");
    print_stats(r.stats.max_run, r.stats.zeros);
    std::printf("  bits taking both values in the first 2^%u outputs: %d of 64\n",
                coverage_log2_outputs, std::popcount(r.both_values));
    std::printf("  bit counts in the first 2^%u outputs: min %" PRIu64 ", max %" PRIu64
                " (limits %" PRIu64 "..%" PRIu64 ")\n",
                balance_log2_outputs, counts.min, counts.max,
                balance_mean - balance_tolerance, balance_mean + balance_tolerance);
    std::printf("  elapsed: %.1f s\n", seconds_since(start));
    std::printf("  %s\n", verdict(pass));
    std::fflush(stdout);
    return pass;
}

bool phase_splitmix()
{
    const auto start = clock_type::now();
    std::printf("splitmix64: seed 0, 2^%u steps\n", splitmix_log2_steps);
    std::fflush(stdout);

    const uint64_t lambda = brent(splitmix64{0}, uint64_t{1} << splitmix_log2_steps,
                                  [](splitmix64& g) { (void)Random::next(g); },
                                  [](const splitmix64& a, const splitmix64& b) { return a.x == b.x; });
    const bool pass = lambda == 0;
    if (lambda != 0)
        std::printf("  lambda: %" PRIu64 "\n", lambda);
    else
        std::printf("  lambda: no cycle\n");
    std::printf("  elapsed: %.1f s\n", seconds_since(start));
    std::printf("  %s\n", verdict(pass));
    std::fflush(stdout);
    return pass;
}

bool run()
{
    const auto start = clock_type::now();
    if (!check_detectors())
    {
        std::printf("detector checks: FAIL\n");
        return false;
    }
    bool pass = phase_a();
    pass &= phase_b();
    pass &= phase_splitmix();
    std::printf("total: %.1f s: %s\n", seconds_since(start), verdict(pass));
    return pass;
}

} // namespace

} // namespace floormat

int main()
{
    return floormat::run() ? 0 : 1;
}
