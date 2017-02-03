#pragma once

/* This is the xorshift128+ generator
 *
 * Title: Further scramblings of Marsaglia's xorshift generators
 * Author: Sebastiano Vigna
 * Date: [v3] Mon, 23 May 2016 15:44:18 GMT (58kb,D)
 * Availability:  arXiv:1404.0390 [cs.DS]
 */

#include <cinttypes>
#include <limits>
#include <tuple>

struct xorshift final
{
    using seed = std::tuple<std::uint64_t, std::uint64_t>;
    static seed make_seed_from_bytes(const void* in, unsigned len);

    using u64 = std::uint64_t;
    using u8 = std::uint8_t;
    using result_type = u64;

    u64 operator()();

    template<typename t>
    void discard(t z)
    {
        for (t k = 0; k < z; k++)
            (void) operator()();
    }

    void reseed(const seed &value);
    seed state() const { return ss; }
    xorshift(u64 s1, u64 s2) : ss(sanitize_seed(seed(s1, s2))) {}
    xorshift(const seed& s = default_seed) : ss(sanitize_seed(s)) {}

    static constexpr u64 min() { return 1u; }
    static constexpr u64 max() { return std::numeric_limits<result_type>::max(); }

    static constexpr seed default_seed = { 0, 0 };

private:
    seed ss;

    static seed sanitize_seed(const seed& s);
};
