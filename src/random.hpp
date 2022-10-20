#pragma once
#include <cstddef>
#include <concepts>
#include <type_traits>

namespace floormat {

struct random_engine
{
    virtual ~random_engine();
    virtual std::common_type_t<std::size_t, std::uintptr_t, std::ptrdiff_t> operator()() = 0;

    template<std::integral T>
    requires (sizeof(T) <= sizeof(std::size_t))
    T operator()(T max) {
        return static_cast<T>(operator()() % static_cast<std::size_t>(max));
    }

    template<std::integral T>
    requires (sizeof(T) <= sizeof(std::size_t))
    T operator()(T min, T max) {
        return min + operator()(max-min);
    }

    virtual float operator()(float min, float max) = 0;
};

[[maybe_unused]] extern random_engine& random; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace floormat
