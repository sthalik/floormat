#include "random.hpp"
#include <random>
#include <limits>

namespace floormat {

static thread_local auto g = std::independent_bits_engine<decltype(std::ranlux48{}), 32, uint32_t>{std::ranlux48{}};

random_engine::~random_engine() = default;

struct random_engine_impl final : random_engine {
    size_t operator()() override;
    float operator()(float min, float max) override;
};

size_t random_engine_impl::operator()()
{
    if constexpr(sizeof(size_t) > sizeof(uint32_t))
    {
        constexpr auto N = (sizeof(size_t) + sizeof(uint32_t)-1) / sizeof(uint32_t);
        static_assert(N >= 1);
        union {
            size_t x;
            uint32_t a[N];
        } ret;
        for (auto i = 0_uz; i < N; i++)
            ret.a[i] = g();
        return ret.x;
    }
    else
        return (size_t)g();
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
