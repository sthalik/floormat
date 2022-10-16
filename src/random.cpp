#include "random.hpp"
#include <random>
#include <limits>

namespace floormat {

static thread_local auto g = std::independent_bits_engine<decltype(std::ranlux48{}), 32, std::uint32_t>{std::ranlux48{}};

struct random_engine_impl final : random_engine {
    std::size_t operator()() override;
    float operator()(float min, float max) override;
};

std::size_t random_engine_impl::operator()()
{
    if constexpr(sizeof(std::size_t) > sizeof(std::uint32_t))
    {
        constexpr std::size_t N = (sizeof(std::size_t) + sizeof(std::uint32_t)-1) / sizeof(std::uint32_t);
        static_assert(N >= 2);
        union {
            std::size_t x;
            std::uint32_t a[N];
        } ret;
#pragma omp unroll full
        for (std::size_t i = 0; i < N; i++)
            ret.a[i] = g();
        return ret.x;
    }
    else
        return (std::size_t)g();
}

float random_engine_impl::operator()(float min, float max)
{
    std::uniform_real_distribution<float> dist{min, max};
    return dist(g);
}

static random_engine& make_random_impl() {
    static random_engine_impl ret;
    return ret;
}
random_engine& random = make_random_impl();

} // namespace floormat
