#include "random/random.hpp"
#include "compat/fwd/chrono.hpp"
#include <bit>
#include <cinttypes>
#include <cstdio>

namespace floormat {

namespace {

using clock_type = std::chrono::steady_clock;
using Random::splitmix64;
using Random::xoshiro256starstar;

constexpr inline u32 phase_a_seeds = 1024;
constexpr inline u32 phase_a_log2_steps = 28;
constexpr inline u64 phase_b_seed = (u64)-1;
constexpr inline u32 phase_b_log2_steps = 36;
constexpr inline u32 splitmix_log2_steps = 32;
constexpr inline u32 masked_bits = 20;
constexpr inline u32 stuck_bit_index = 37;

constexpr inline u64 run_limit = 2;
constexpr inline u64 zero_limit = 2;
constexpr inline u32 coverage_log2_outputs = 16;
constexpr inline u32 balance_log2_outputs = 24;
constexpr inline u64 balance_mean = u64{1} << (balance_log2_outputs - 1);
// 6 standard deviations of a bit count, sqrt(2^24 / 4) = 2048.
constexpr inline u64 balance_tolerance = 6 * 2048;

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
u64 brent(State hare, u64 max_steps, Step&& step, Equal&& equal)
{
    State tortoise = hare;
    step(hare);
    u64 power = 1, lambda = 1, steps = 1;
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
    u64 zeros = 0, prev = 0, run = 0, max_run = 0;

    void add(u64 x)
    {
        zeros += x == 0;
        run = x == prev ? run + 1 : 1;
        prev = x;
        if (run > max_run)
            max_run = run;
    }
};

template<typename Next>
u64 bits_taking_both_values(xoshiro256starstar g, u64 count, Next&& next)
{
    u64 any = 0, all = (u64)-1;
    for (u64 i = 0; i < count; i++)
    {
        const u64 x = next(g);
        any |= x;
        all &= x;
    }
    return any & ~all;
}

struct bit_count_range
{
    u64 min, max;
};

template<typename Next>
bit_count_range count_bits(xoshiro256starstar g, u64 count, Next&& next)
{
    u64 counts[64] = {};
    for (u64 i = 0; i < count; i++)
    {
        const u64 x = next(g);
        for (u32 b = 0; b < 64; b++)
            counts[b] += x >> b & 1;
    }
    bit_count_range r{(u64)-1, 0};
    for (u64 c : counts)
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
    u64 lambda;
    output_stats stats;
    u64 both_values;
};

template<typename Next>
seed_result test_seed(const xoshiro256starstar& start, u64 steps, Next&& next)
{
    output_stats stats;
    const u64 lambda = brent(start, steps, [&](xoshiro256starstar& g) { stats.add(next(g)); }, same_state);
    return { lambda, stats, bits_taking_both_values(start, u64{1} << coverage_log2_outputs, next) };
}

bool runs_ok(const output_stats& stats)
{
    return stats.max_run <= run_limit;
}

bool coverage_ok(u64 both_values)
{
    return both_values == (u64)-1;
}

bool balance_ok(bit_count_range r)
{
    return r.min >= balance_mean - balance_tolerance && r.max <= balance_mean + balance_tolerance;
}

bool seed_ok(const seed_result& r)
{
    return r.lambda == 0 && runs_ok(r.stats) && r.stats.zeros <= zero_limit && coverage_ok(r.both_values);
}

u64 plain_output(xoshiro256starstar& g)
{
    return Random::next(g);
}

// Advances the state so that brent() doesn't stop after the first output.
u64 constant_output(xoshiro256starstar& g)
{
    (void)Random::next(g);
    return 0x0123456789abcdef;
}

u64 stuck_bit_output(xoshiro256starstar& g)
{
    return Random::next(g) & ~(u64{1} << stuck_bit_index);
}

bool check_detectors()
{
    bool ok = true;

    {
        constexpr u64 mask = (u64{1} << masked_bits) - 1;
        const auto start = clock_type::now();
        // brent() finds a cycle of length 2^k after 2^(k+1) - 1 steps.
        const u64 lambda = brent(splitmix64{0}, u64{1} << (masked_bits + 1),
                                      [](splitmix64& g) { (void)Random::next(g); g.x &= mask; },
                                      [](const splitmix64& a, const splitmix64& b) { return a.x == b.x; });
        const bool pass = lambda == u64{1} << masked_bits;
        std::printf("detector check: splitmix64 with its state masked to %u bits\n", masked_bits);
        std::printf("  lambda: %" PRIu64 " (expected 2^%u)\n", lambda, masked_bits);
        std::printf("  elapsed: %.2f s\n", seconds_since(start));
        std::printf("  %s\n", verdict(pass));
        ok &= pass;
    }

    {
        const auto r = test_seed(xoshiro256starstar{}, u64{1} << coverage_log2_outputs, constant_output);
        const bool pass = !runs_ok(r.stats);
        std::printf("detector check: constant output, the run check must fail\n");
        std::printf("  max run of identical outputs: %" PRIu64 " (limit %" PRIu64 ")\n", r.stats.max_run, run_limit);
        std::printf("  %s\n", verdict(pass));
        ok &= pass;
    }

    {
        const auto start = clock_type::now();
        const u64 both = bits_taking_both_values(xoshiro256starstar{}, u64{1} << coverage_log2_outputs,
                                                      stuck_bit_output);
        const auto counts = count_bits(xoshiro256starstar{}, u64{1} << balance_log2_outputs, stuck_bit_output);
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

void print_stats(u64 max_run, u64 zeros)
{
    std::printf("  max run of identical outputs: %" PRIu64 " (limit %" PRIu64 ")\n", max_run, run_limit);
    std::printf("  zero outputs: %" PRIu64 " (limit %" PRIu64 ")\n", zeros, zero_limit);
}

bool phase_a()
{
    const auto start = clock_type::now();
    const u64 steps = u64{1} << phase_a_log2_steps;
    std::printf("phase A: seeds 0..%u, 2^%u steps each\n", phase_a_seeds - 1, phase_a_log2_steps);
    std::fflush(stdout);

    u64 max_run = 0, zeros = 0;
    u32 cycles = 0, failed_seeds = 0;
    int min_both = 64;
    for (u32 seed = 0; seed < phase_a_seeds; seed++)
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
    const auto r = test_seed(g, u64{1} << phase_b_log2_steps, plain_output);
    const auto counts = count_bits(g, u64{1} << balance_log2_outputs, plain_output);
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

    const u64 lambda = brent(splitmix64{0}, u64{1} << splitmix_log2_steps,
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
