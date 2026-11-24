#include "app.hpp"
#include "compat/sequence-traits.hpp"
#include <array>
#include <tuple>
#include <cr/Array.h>
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

static_assert(random_access_sequence<std::tuple<>>);
static_assert(random_access_sequence<std::tuple<u8, i16, float>>);
static_assert(random_access_sequence<const std::pair<i32, char>>);
static_assert(random_access_sequence<Containers::Pair<i8, double>>);
static_assert(random_access_sequence<Containers::Triple<u16, i64, float>>);
static_assert(random_access_sequence<Math::Vector<5, i32>>);
static_assert(random_access_sequence<Vector2i>);
static_assert(random_access_sequence<const Vector3>);
static_assert(random_access_sequence<Vector4ui>);
static_assert(random_access_sequence<Color3ub>);
static_assert(random_access_sequence<Color4>);
static_assert(random_access_sequence<std::array<u8, 3>>);
static_assert(random_access_sequence<const std::array<float, 0>>);
static_assert(random_access_sequence<StaticArray<4, i32>>);
static_assert(random_access_sequence<i32[3]>);
static_assert(random_access_sequence<const u8[2]>);

static_assert(!random_access_sequence<i32>);
static_assert(!random_access_sequence<float>);
static_assert(!random_access_sequence<Containers::Optional<i32>>);
static_assert(!random_access_sequence<Containers::Array<i32>>);
static_assert(!random_access_sequence<Math::Matrix3<float>>);

static_assert(sequence_traits<std::tuple<>>::size == 0);
static_assert(sequence_traits<std::tuple<u8, i16, float>>::size == 3);
static_assert(sequence_traits<std::pair<i32, char>>::size == 2);
static_assert(sequence_traits<Containers::Triple<u16, i64, float>>::size == 3);
static_assert(sequence_traits<Math::Vector<5, i32>>::size == 5);
static_assert(sequence_traits<Color4ub>::size == 4);
static_assert(sequence_traits<std::array<u8, 3>>::size == 3);
static_assert(sequence_traits<std::array<float, 0>>::size == 0);
static_assert(sequence_traits<StaticArray<4, i32>>::size == 4);
static_assert(sequence_traits<i32[3]>::size == 3);

static_assert(std::is_same_v<sequence_element_t<std::tuple<u8, i16, float>, 1>, i16>);
static_assert(std::is_same_v<sequence_element_t<std::pair<i8, const u16>, 1>, u16>);
static_assert(std::is_same_v<sequence_element_t<const Containers::Pair<i8, double>, 0>, i8>);
static_assert(std::is_same_v<sequence_element_t<Containers::Triple<u16, i64, float>, 2>, float>);
static_assert(std::is_same_v<sequence_element_t<Color3ub, 2>, UnsignedByte>);
static_assert(std::is_same_v<sequence_element_t<Math::Vector<5, i32>, 4>, i32>);
static_assert(std::is_same_v<sequence_element_t<const std::array<i16, 2>, 1>, i16>);
static_assert(std::is_same_v<sequence_element_t<StaticArray<3, double>, 0>, double>);
static_assert(std::is_same_v<sequence_element_t<const u8[2], 1>, u8>);

constexpr bool test_get()
{
    constexpr auto t = std::tuple{u8{101}, i16{-103}, 107.5f};
    constexpr auto p = Containers::pair(i8{-109}, 113.25);
    constexpr auto q = Containers::triple(u16{127}, i64{-131}, 137.75f);
    constexpr auto v = Color3ub{139, 149, 151};
    constexpr auto a = std::array<i16, 2>{-257, 263};
    constexpr auto s = StaticArray<3, u32>{269u, 271u, 277u};
    constexpr i64 c[2] = {-281, 283};
    using T = sequence_traits<std::remove_cv_t<decltype(t)>>;
    using P = sequence_traits<std::remove_cv_t<decltype(p)>>;
    using Q = sequence_traits<std::remove_cv_t<decltype(q)>>;
    using V = sequence_traits<Color3ub>;
    using A = sequence_traits<std::remove_cv_t<decltype(a)>>;
    using S = sequence_traits<std::remove_cv_t<decltype(s)>>;
    using C = sequence_traits<std::remove_cv_t<decltype(c)>>;
    return T::get<0>(t) == 101 && T::get<1>(t) == -103 && T::get<2>(t) == 107.5f &&
           P::get<0>(p) == -109 && P::get<1>(p) == 113.25 &&
           Q::get<0>(q) == 127 && Q::get<1>(q) == -131 && Q::get<2>(q) == 137.75f &&
           V::get<0>(v) == 139 && V::get<1>(v) == 149 && V::get<2>(v) == 151 &&
           A::get<0>(a) == -257 && A::get<1>(a) == 263 &&
           S::get<0>(s) == 269 && S::get<1>(s) == 271 && S::get<2>(s) == 277 &&
           C::get<0>(c) == -281 && C::get<1>(c) == 283;
}

constexpr auto size_of_element = [](auto to, const auto&) {
    using T = decltype(to)::type;
    return T(sizeof(T));
};

constexpr auto double_plus_one = [](auto to, const auto& x) {
    using T = decltype(to)::type;
    return T(x*2 + 1);
};

constexpr bool test_transform()
{
    const auto a = transform_elements<std::tuple<u8, u16, u32, u64>>(std::tuple{157, 163, 167, 173},
                                                                      size_of_element);
    const auto b = transform_elements<std::pair<i32, double>>(std::pair{u8{179}, 181.25f}, double_plus_one);
    const auto c = transform_elements<Vector3i>(Vector3ub{191, 193, 197}, double_plus_one);
    const auto d = transform_elements<Containers::Triple<u16, i64, float>>(Vector3i{199, -211, 223},
                                                                            double_plus_one);
    const auto e = transform_elements<Vector2d>(Containers::pair(i16{-227}, 229.5f), double_plus_one);
    const auto f = transform_elements<std::tuple<>>(std::tuple<>{}, double_plus_one);
    const auto g = transform_elements<const Color4ub>(std::tuple{233, 239, 241, 251}, size_of_element);
    const auto h = transform_elements<StaticArray<3, i64>>(std::array<i8, 3>{-7, 11, 13}, double_plus_one);
    const auto i = transform_elements<std::array<u16, 2>>(StaticArray<2, u8>{u8{17}, u8{19}}, double_plus_one);
    const auto j = transform_elements<Vector3i>(StaticArray<3, i16>{i16{-23}, i16{29}, i16{31}}, double_plus_one);
    const auto k = transform_elements<std::array<float, 0>>(std::tuple<>{}, double_plus_one);
    const i32 l_from[3] = {37, -41, 43};
    const auto l = transform_elements<Containers::Triple<i16, i32, i64>>(l_from, double_plus_one);
    return std::get<0>(a) == 1 && std::get<1>(a) == 2 && std::get<2>(a) == 4 && std::get<3>(a) == 8 &&
           b.first == 359 && b.second == 363.5 &&
           c == Vector3i{383, 387, 395} &&
           d.first() == 399 && d.second() == -421 && d.third() == 447.f &&
           e == Vector2d{-453., 460.} &&
           f == std::tuple<>{} &&
           g == Color4ub{1, 1, 1, 1} &&
           h[0] == -13 && h[1] == 23 && h[2] == 27 &&
           i == std::array<u16, 2>{35, 39} &&
           j == Vector3i{-45, 59, 63} &&
           k.empty() &&
           l.first() == 75 && l.second() == -81 && l.third() == 87;
}

static_assert(test_get());
static_assert(test_transform());

} // namespace

void test_sequence_traits()
{
    fm_assert(test_get());
    fm_assert(test_transform());
}

} // namespace floormat::Test
