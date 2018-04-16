#pragma once

#include <memory>
#include <cmath>
#include <utility>
#include <type_traits>

#include "macros.hpp"

#include <QDebug>

#define progn(...) ([&]() { __VA_ARGS__ }())
template<typename t, typename... xs> using ptr = std::unique_ptr<t, xs...>;

#ifdef Q_CREATOR_RUN
#   define DEFUN_WARN_UNUSED
#elif defined(_MSC_VER)
#   define DEFUN_WARN_UNUSED _Check_return_
#else
#   define DEFUN_WARN_UNUSED __attribute__((warn_unused_result))
#endif

template<typename t>
int iround(const t& val)
{
    return int(std::round(val));
}

namespace util_detail {

template<typename n>
inline auto clamp_float(n val, n min, n max)
{
    return std::fmin(std::fmax(val, min), max);
}

template<typename t, typename n>
struct clamp final
{
    static inline auto clamp_(const n& val, const n& min, const n& max)
    {
        if (unlikely(val > max))
            return max;
        if (unlikely(val < min))
            return min;
        return val;
    }
};

template<typename t>
struct clamp<float, t>
{
    static inline auto clamp_(float val, float min, float max)
    {
        return clamp_float(val, min, max);
    }
};

template<typename t>
struct clamp<double, t>
{
    static inline auto clamp_(double val, double min, double max)
    {
        return clamp_float(val, min, max);
    }
};

} // ns util_detail

template<typename t, typename u, typename w>
inline auto clamp(const t& val, const u& min, const w& max)
{
    using tp = decltype(val + min + max);
    return ::util_detail::clamp<std::decay_t<tp>, tp>::clamp_(val, min, max);
}

