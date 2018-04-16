#pragma once

/*  Written in 2016 by David Blackman and Sebastiano Vigna (vigna@acm.org)

    To the extent possible under law, the author has dedicated all copyright
    and related and neighboring rights to this software to the public domain
    worldwide. This software is distributed without any warranty.

    See <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <cstdint>
#include <ctime>
#include <tuple>
#include <limits>
#include <utility>

struct xoroshiro final
{
    xoroshiro& operator=(const xoroshiro&) = delete;

    using seed = std::pair<std::uint64_t, std::uint64_t>;

    using u64 = std::uint64_t;
    using result_type = u64;

    u64 operator()();

    seed state() const { return ss; }

    xoroshiro(const seed& s) : ss(s) {}
    xoroshiro(const u64& s = std::time(nullptr));
    xoroshiro(const void* buf, unsigned len);

    static inline constexpr result_type xoroshiro::min() { return 0; }
    static inline constexpr result_type xoroshiro::max() { return std::numeric_limits<result_type>::max(); }

private:
    static seed make_seed_from_bytes(const void* in, unsigned len);

    seed ss;
};

