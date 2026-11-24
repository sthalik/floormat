#include "app.hpp"
#include "compat/saturate-cast.hpp"
#include "compat/limits.hpp"
#include <array>
#include <tuple>
#include <cr/Optional.h>
#include <cr/Pair.h>
#include <cr/StaticArray.h>
#include <cr/Triple.h>
#include <mg/Color.h>
#include <mg/Vector2.h>
#include <mg/Vector3.h>
#include <mg/Vector4.h>

namespace floormat::Test {

namespace {

template<typename To, typename From>
concept castable = requires (From&& value) { saturate_cast<To, From>(forward<From>(value)); };

template<typename To, typename From>
constexpr bool test_integer_pair()
{
    using T = limits<To>;
    using F = limits<From>;
    if (saturate_cast<To>(From{0}) != To{0} || saturate_cast<To>(From{1}) != To{1})
        return false;
    if constexpr (T::digits >= 7 && F::digits >= 7)
    {
        if (saturate_cast<To>(From{97}) != To{97})
            return false;
    }
    if constexpr (std::is_signed_v<To> && std::is_signed_v<From>)
    {
        if (saturate_cast<To>(From{-89}) != To{-89})
            return false;
    }
    if constexpr (T::digits >= F::digits)
    {
        if (saturate_cast<To>(F::max) != static_cast<To>(F::max))
            return false;
    }
    else
    {
        if (saturate_cast<To>(static_cast<From>(T::max)) != T::max ||
            saturate_cast<To>(static_cast<From>(static_cast<From>(T::max) + From{1})) != T::max ||
            saturate_cast<To>(F::max) != T::max)
            return false;
    }
    if constexpr (std::is_signed_v<From> && !std::is_signed_v<To>)
    {
        if (saturate_cast<To>(From{-1}) != To{0} || saturate_cast<To>(F::min) != To{0})
            return false;
    }
    else if constexpr (std::is_signed_v<From> && T::digits >= F::digits)
    {
        if (saturate_cast<To>(F::min) != static_cast<To>(F::min))
            return false;
    }
    else if constexpr (std::is_signed_v<From>)
    {
        if (saturate_cast<To>(static_cast<From>(T::min)) != T::min ||
            saturate_cast<To>(static_cast<From>(static_cast<From>(T::min) - From{1})) != T::min ||
            saturate_cast<To>(F::min) != T::min)
            return false;
    }
    return true;
}

template<typename To>
constexpr bool test_integer_sources()
{
    static_assert(!requires (float value) { saturate_cast<To>(value); });
    static_assert(!requires (double value) { saturate_cast<To>(value); });
    static_assert(!requires (long double value) { saturate_cast<To>(value); });
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
           test_integer_sources<unsigned long long>() &&
           saturate_cast<const u8>(-331) == 0 && saturate_cast<const u8>(337) == 255;
}

constexpr bool test_floats()
{
    return saturate_cast<float>(16777217) == 0x1p24f &&
           saturate_cast<float>(-16777217) == -0x1p24f &&
           saturate_cast<float>(16777213) == 16777213.f &&
           saturate_cast<float>(i64{-1} << 40) == -0x1p24f &&
           saturate_cast<float>(limits<u32>::max) == 0x1p24f &&
           saturate_cast<float>(limits<i16>::max) == 32767.f &&
           saturate_cast<float>(limits<i16>::min) == -32768.f &&
           saturate_cast<float>(u8{251}) == 251.f &&
           saturate_cast<double>(u64{9007199254740993}) == 0x1p53 &&
           saturate_cast<double>(-9007199254740993LL) == -0x1p53 &&
           saturate_cast<double>(limits<i32>::min) == -2147483648. &&
           saturate_cast<float>(16777219.) == 0x1p24f &&
           saturate_cast<float>(-16777219.) == -0x1p24f &&
           saturate_cast<float>(0x1p60) == 0x1p24f &&
           saturate_cast<float>(-0x1p60) == -0x1p24f &&
           saturate_cast<float>(37.5) == 37.5f &&
           saturate_cast<float>(-41.25) == -41.25f &&
           saturate_cast<double>(0x1p100f) == 0x1p100 &&
           saturate_cast<double>(0x1p60L) == (LDBL_MANT_DIG > DBL_MANT_DIG ? 0x1p53 : 0x1p60) &&
           saturate_cast<long double>(limits<u64>::max) == (LDBL_MANT_DIG >= 64 ? 0x1p64L - 1 : 0x1p53L);
}

constexpr bool test_vectors()
{
    const auto a = saturate_cast<Vector2b>(Vector2i{-131, 137});
    const auto b = saturate_cast<Vector3ub>(Vector3i{-7, 139, 263});
    const auto c = saturate_cast<Math::Vector<5, i16>>(Math::Vector<5, int>{40009, -40013, 151, -157, 32767});
    const auto d = saturate_cast<Color4ub>(Vector4ui{163, 257, 0, 4294967291u});
    const auto e = saturate_cast<Vector2ui>(Vector2i{-167, 173});
    const auto f = saturate_cast<Vector2>(Vector2d{16777219., -41.25});
    return a == Vector2b{-128, 127} && b == Vector3ub{0, 139, 255} &&
           c[0] == 32767 && c[1] == -32768 && c[2] == 151 && c[3] == -157 && c[4] == 32767 &&
           d == Color4ub{163, 255, 0, 255} && e == Vector2ui{0, 173} &&
           f.x() == 0x1p24f && f.y() == -41.25f;
}

constexpr bool test_tuples()
{
    using To = std::tuple<u16, Vector2, std::tuple<i8, Vector3ub>>;
    const auto a = saturate_cast<To>(std::tuple{-179, Vector2d{-16777221., 47.5},
                                                std::tuple{181L, Vector3i{191, -193, 271}}});
    const auto b = saturate_cast<std::tuple<std::tuple<u8>>>(std::tuple<std::tuple<int>>{{269}});
    return std::get<0>(a) == 0 && std::get<1>(a).x() == -0x1p24f && std::get<1>(a).y() == 47.5f &&
           std::get<0>(std::get<2>(a)) == 127 &&
           std::get<1>(std::get<2>(a)) == Vector3ub{191, 0, 255} &&
           std::get<0>(std::get<0>(b)) == 255 &&
           saturate_cast<std::tuple<>>(std::tuple<>{}) == std::tuple<>{};
}

constexpr bool test_other_sequences()
{
    const auto a = saturate_cast<std::pair<u8, std::tuple<i16, Vector2ub>>>(
        std::pair{271, std::tuple{-40009L, Vector2i{-17, 277}}});
    const auto b = saturate_cast<Containers::Triple<u8, i8, u16>>(Vector3i{-283, 293, 70001});
    const auto c = saturate_cast<Vector2b>(std::tuple{-307, 311});
    const auto d = saturate_cast<std::pair<u8, u8>>(Containers::pair(313L, -317L));
    const auto e = saturate_cast<StaticArray<3, u8>>(std::array<i32, 3>{-331, 337, 347});
    const auto f = saturate_cast<std::array<i8, 2>>(StaticArray<2, i64>{-349, 53});
    const i32 g_from[2] = {353, -359};
    const auto g = saturate_cast<Vector2b>(g_from);
    static_assert(!castable<Vector2ub, Vector3i>);
    static_assert(!castable<std::pair<u8, u8>, std::tuple<int>>);
    static_assert(!castable<std::tuple<u8>, Containers::Optional<int>>);
    static_assert(castable<Vector2ub, std::tuple<int, long>>);
    static_assert(!castable<std::array<u8, 2>, std::array<int, 3>>);
    static_assert(castable<StaticArray<2, u8>, Vector2i>);
    static_assert(!castable<i32[2], Vector2i>);
    static_assert(!castable<int&, int>);
    static_assert(!castable<int, int&>);
    static_assert(!castable<int&, int&>);
    return a.first == 255 && std::get<0>(a.second) == -32768 && std::get<1>(a.second) == Vector2ub{0, 255} &&
           b.first() == 0 && b.second() == 127 && b.third() == 65535 &&
           c == Vector2b{-128, 127} && d == std::pair<u8, u8>{255, 0} &&
           e[0] == 0 && e[1] == 255 && e[2] == 255 && f == std::array<i8, 2>{-128, 53} &&
           g == Vector2b{127, -128};
}

static_assert(test_integers());
static_assert(test_floats());
static_assert(test_vectors());
static_assert(test_tuples());
static_assert(test_other_sequences());

} // namespace

void test_saturate_cast()
{
    fm_assert(test_integers());
    fm_assert(test_floats());
    fm_assert(test_vectors());
    fm_assert(test_tuples());
    fm_assert(test_other_sequences());
}

} // namespace floormat::Test
