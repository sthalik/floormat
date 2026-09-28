#include "random.hpp"
#include <bit>
#if defined __SSE2__ || defined _M_X64 || (defined _M_IX86_FP && _M_IX86_FP >= 2)
#include <emmintrin.h>
#else
#include <cmath>
#endif

namespace floormat::Random {

namespace {

// mingw-w64's sqrt rounds wrongly for about 1 in 4000 inputs, and GCC at -O0 calls it.
double sqrt_(double x)
{
#if defined __SSE2__ || defined _M_X64 || (defined _M_IX86_FP && _M_IX86_FP >= 2)
    return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(x)));
#else
    return std::sqrt(x);
#endif
}

// Wichura, Algorithm AS 241 (PPND16), Applied Statistics 37 (1988), lowest degree first.
constexpr double a[] = {
    3.3871328727963666080e+0, 1.3314166789178437745e+2, 1.9715909503065514427e+3, 1.3731693765509461125e+4,
    4.5921953931549871457e+4, 6.7265770927008700853e+4, 3.3430575583588128105e+4, 2.5090809287301226727e+3,
};
constexpr double b[] = {
    1, 4.2313330701600911252e+1, 6.8718700749205790830e+2, 5.3941960214247511077e+3,
    2.1213794301586595867e+4, 3.9307895800092710610e+4, 2.8729085735721942674e+4, 5.2264952788528545610e+3,
};
constexpr double c[] = {
    1.42343711074968357734e+0, 4.63033784615654529590e+0, 5.76949722146069140550e+0, 3.64784832476320460504e+0,
    1.27045825245236838258e+0, 2.41780725177450611770e-1, 2.27238449892691845833e-2, 7.74545014278341407640e-4,
};
constexpr double d[] = {
    1, 2.05319162663775882187e+0, 1.67638483018380384940e+0, 6.89767334985100004550e-1,
    1.48103976427480074590e-1, 1.51986665636164571966e-2, 5.47593808499534494600e-4, 1.05075007164441684324e-9,
};
constexpr double e[] = {
    6.65790464350110377720e+0, 5.46378491116411436990e+0, 1.78482653991729133580e+0, 2.96560571828504891230e-1,
    2.65321895265761230930e-2, 1.24266094738807843860e-3, 2.71155556874348757815e-5, 2.01033439929228813265e-7,
};
constexpr double f[] = {
    1, 5.99832206555887937690e-1, 1.36929880922735805310e-1, 1.48753612908506148525e-2,
    7.86869131145613259100e-4, 1.84631831751005468180e-5, 1.42151175831644588870e-7, 2.04426310338993978564e-15,
};

template<uint32_t N>
double polynomial(double x, const double (&coeffs)[N])
{
    double y = coeffs[N-1];
    for (uint32_t i = N-1; i-- > 0; )
        y = y * x + coeffs[i];
    return y;
}

// libm's log rounds differently in each C library. Only for normal x > 0.
double log_positive(double x)
{
    const uint64_t bits = std::bit_cast<uint64_t>(x);
    int32_t exponent = int32_t(bits >> 52) - 1023;
    double m = std::bit_cast<double>((bits & 0x000fffffffffffff) | 0x3ff0000000000000);
    if (m > 1.4142135623730951)
    {
        m *= 0.5;
        exponent++;
    }
    // log(m) = 2 atanh(s), and s^2 < 0.0295 needs 11 terms of the series.
    constexpr double atanh_series[] = {
        1, 1.0/3, 1.0/5, 1.0/7, 1.0/9, 1.0/11, 1.0/13, 1.0/15, 1.0/17, 1.0/19, 1.0/21,
    };
    const double s = (m - 1) / (m + 1);
    return exponent * 0.6931471805599453 + 2 * s * polynomial(s * s, atanh_series);
}

// Inverse normal CDF of (m + 1/2) / 2^64 for m < 2^63.
double lower_quantile(uint64_t m)
{
    // p - 1/2 from a rounded p loses q's low bits near p = 1/2.
    const double q = -((double)int64_t(0x7fffffffffffffff - m) + 0.5) * 0x1p-64;
    if (q >= -0.425)
    {
        const double r = 0.180625 - q * q;
        return q * polynomial(r, a) / polynomial(r, b);
    }
    const double p = ((double)int64_t(m) + 0.5) * 0x1p-64;
    const double r = sqrt_(-log_positive(p));
    if (r <= 5)
        return -polynomial(r - 1.6, c) / polynomial(r - 1.6, d);
    else
        return -polynomial(r - 5, e) / polynomial(r - 5, f);
}

} // namespace

template<> double gaussian<double>(uint64_t u)
{
    const bool upper = u >> 63;
    const double z = lower_quantile(upper ? ~u : u);
    return upper ? -z : z;
}

template<> float gaussian<float>(uint64_t u)
{
    return (float)gaussian<double>(u);
}

template<> double gaussian<double>(uint64_t u, double center, double spread)
{
    return center + spread * gaussian<double>(u);
}

template<> float gaussian<float>(uint64_t u, float center, float spread)
{
    return (float)gaussian<double>(u, (double)center, (double)spread);
}

} // namespace floormat::Random
