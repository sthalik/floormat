#include "app.hpp"
#include "compat/checked-cast.hpp"
#include "compat/limits.hpp"
#include <tuple>
#include <mg/Vector2.h>
#include <mg/Vector3.h>
#include <mg/Vector4.h>

namespace floormat::Test {

namespace {

template<typename To, auto Value>
concept constant_castable = requires {
    typename std::bool_constant<(static_cast<void>(checked_cast<To>(Value())), true)>;
};

template<typename To, typename From>
constexpr bool test_integer_pair()
{
    using T = limits<To>;
    using F = limits<From>;
    if (checked_cast<To>(From{0}) != To{0} || checked_cast<To>(From{1}) != To{1})
        return false;
    if constexpr (T::digits >= F::digits)
    {
        if (checked_cast<To>(F::max) != static_cast<To>(F::max))
            return false;
    }
    else
    {
        if (checked_cast<To>(static_cast<From>(T::max)) != T::max)
            return false;
    }
    if constexpr (std::is_signed_v<To> && std::is_signed_v<From>)
    {
        if constexpr (T::digits >= F::digits)
        {
            if (checked_cast<To>(F::min) != static_cast<To>(F::min))
                return false;
        }
        else
        {
            if (checked_cast<To>(static_cast<From>(T::min)) != T::min)
                return false;
            static_assert(!constant_castable<To, [] {
                return static_cast<From>(static_cast<From>(T::min) - From{1});
            }>);
        }
    }
    if constexpr (std::is_signed_v<From> && !std::is_signed_v<To>)
        static_assert(!constant_castable<To, [] { return From{-1}; }>);
    if constexpr (T::digits < F::digits)
        static_assert(!constant_castable<To, [] { return static_cast<From>(static_cast<From>(T::max) + From{1}); }>);
    return true;
}

template<typename To>
constexpr bool test_integer_sources()
{
    static_assert(!requires (float value) { checked_cast<To>(value); });
    static_assert(!requires (double value) { checked_cast<To>(value); });
    static_assert(!requires (long double value) { checked_cast<To>(value); });
    return test_integer_pair<To, bool>() &&
           test_integer_pair<To, char>() &&
           test_integer_pair<To, signed char>() &&
           test_integer_pair<To, unsigned char>() &&
           test_integer_pair<To, short>() &&
           test_integer_pair<To, unsigned short>() &&
           test_integer_pair<To, int>() &&
           test_integer_pair<To, unsigned>() &&
           test_integer_pair<To, long>() &&
           test_integer_pair<To, unsigned long>() &&
           test_integer_pair<To, long long>() &&
           test_integer_pair<To, unsigned long long>();
}

constexpr bool test_integers()
{
    return test_integer_sources<bool>() &&
           test_integer_sources<char>() &&
           test_integer_sources<signed char>() &&
           test_integer_sources<unsigned char>() &&
           test_integer_sources<short>() &&
           test_integer_sources<unsigned short>() &&
           test_integer_sources<int>() &&
           test_integer_sources<unsigned>() &&
           test_integer_sources<long>() &&
           test_integer_sources<unsigned long>() &&
           test_integer_sources<long long>() &&
           test_integer_sources<unsigned long long>();
}

template<typename From>
constexpr bool test_float_source()
{
    using F = limits<From>;
    if constexpr (F::digits > limits<float>::digits)
        static_assert(!constant_castable<float, [] { return F::max; }>);
    if constexpr (F::digits > limits<double>::digits)
        static_assert(!constant_castable<double, [] { return F::min; }>);
    return checked_cast<float>(From{37.5L}) == 37.5f &&
           checked_cast<double>(From{-41.25L}) == -41.25 &&
           checked_cast<long double>(From{43.75L}) == 43.75L;
}

constexpr bool test_floats()
{
    static_assert(constant_castable<const float, [] { return 17; }>);
    static_assert(constant_castable<const double, [] { return 23; }>);
    static_assert(constant_castable<const long double, [] { return 29; }>);
    static_assert(LDBL_MANT_DIG != 53 || limits<long double>::max == 0x1p53L);
    static_assert(LDBL_MANT_DIG != 64 || limits<long double>::max == 0x1p64L);
    static_assert(LDBL_MANT_DIG != 113 || limits<long double>::max == 0x1p113L);
    static_assert(constant_castable<long double, [] { return limits<u64>::max; }> == (LDBL_MANT_DIG >= 64));
    static_assert(!constant_castable<float, [] { return 16777217; }>);
    static_assert(!constant_castable<float, [] { return -16777217; }>);
    static_assert(!constant_castable<float, [] { return 16777217.; }>);
    static_assert(!constant_castable<double, [] { return u64{9007199254740993}; }>);
    static_assert(!constant_castable<double, [] { return -9007199254740993LL; }>);
    static_assert(!constant_castable<Vector2, [] { return Vector2d{17, 16777217.}; }>);
    static_assert(!constant_castable<std::tuple<double>, [] { return std::tuple{u64{9007199254740993}}; }>);
    return test_float_source<float>() && test_float_source<double>() && test_float_source<long double>() &&
           checked_cast<double>(u64{1} << 53) == 0x1p53 &&
           checked_cast<double>(i64{-9007199254740992}) == -0x1p53 &&
           checked_cast<float>(u32{16777216}) == 0x1p24f &&
           checked_cast<float>(i64{-16777216}) == -0x1p24f &&
           checked_cast<long double>(limits<i32>::max) == 2147483647.L &&
           checked_cast<float>(static_cast<double>(limits<float>::max)) ==
               limits<float>::max;
}

constexpr bool test_vectors()
{
    const auto a = checked_cast<Vector2ui>(Vector2i{17, 23});
    const auto b = checked_cast<Vector3>(Vector3d{-29.75, 31.5, 37.25});
    const auto c = checked_cast<Vector4ub>(Vector4i{41, 43, 47, 53});
    const auto d = checked_cast<Math::Vector<5, i16>>(Math::Vector<5, int>{59, -61, 67, -71, 73});
    static_assert(!constant_castable<Vector3ub, [] { return Vector3i{79, -1, 83}; }>);
    static_assert(!constant_castable<Vector2b, [] { return Vector2i{89, 128}; }>);
    static_assert(!constant_castable<Vector4, [] { return Vector4d{97, 101, 16777217., 103}; }>);
    return a == Vector2ui{17, 23} && b == Vector3{-29.75f, 31.5f, 37.25f} && c == Vector4ub{41, 43, 47, 53} &&
           d[0] == 59 && d[1] == -61 && d[2] == 67 && d[3] == -71 && d[4] == 73;
}

constexpr bool test_tuples()
{
    using To = std::tuple<u16, Vector2, std::tuple<i8, Math::Vector<3, double>>>;
    const auto src = std::tuple{107, Vector2d{-109.75, 113.5}, std::tuple{127L, Vector3i{131, 137, 139}}};
    const auto dst = checked_cast<To>(src);
    static_assert(!constant_castable<To, [] {
        return std::tuple{107, Vector2d{-109.75, 113.5}, std::tuple{128L, Vector3i{131, 137, 139}}};
    }>);
    static_assert(!constant_castable<std::tuple<std::tuple<u8>>, [] {
        return std::tuple<std::tuple<int>>{{-1}};
    }>);
    return std::get<0>(dst) == 107 && std::get<1>(dst) == Vector2{-109.75f, 113.5f} &&
           std::get<0>(std::get<2>(dst)) == 127 &&
           std::get<1>(std::get<2>(dst)) == Math::Vector<3, double>{131., 137., 139.} &&
           checked_cast<std::tuple<>>(std::tuple<>{}) == std::tuple<>{};
}

static_assert(test_integers());
static_assert(test_floats());
static_assert(test_vectors());
static_assert(test_tuples());

} // namespace

void test_checked_cast()
{
    fm_assert(test_integers());
    fm_assert(test_floats());
    fm_assert(test_vectors());
    fm_assert(test_tuples());
}

} // namespace floormat::Test
