#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Color.h>
#include <nlohmann/json.hpp>

namespace nlohmann {

template<std::size_t N, typename T>
struct adl_serializer<Magnum::Math::Vector<N, T>> {
    using vec = Magnum::Math::Vector<N, T>;
    static void to_json(json& j, const vec& val);
    static void from_json(const json& j, vec& val);
};

template <std::size_t N, typename T>
void adl_serializer<Magnum::Math::Vector<N, T>>::to_json(json& j, const vec& val)
{
    std::array<T, N> array{};
    for (std::size_t i = 0; i < N; i++)
        array[i] = val[i];
    j = array;
}

template <std::size_t N, typename T>
void adl_serializer<Magnum::Math::Vector<N, T>>::from_json(const json& j, vec& val)
{
    std::array<T, N> array = j;
    for (std::size_t i = 0; i < N; i++)
        val[i] = array[i];
}

template<typename T> struct adl_serializer<Magnum::Math::Vector3<T>> : adl_serializer<Magnum::Math::Vector<3, T>> {};
template<typename T> struct adl_serializer<Magnum::Math::Vector4<T>> : adl_serializer<Magnum::Math::Vector<4, T>> {};
template<typename T> struct adl_serializer<Magnum::Math::Color3<T>>  : adl_serializer<Magnum::Math::Vector<3, T>> {};
template<typename T> struct adl_serializer<Magnum::Math::Color4<T>>  : adl_serializer<Magnum::Math::Vector<4, T>> {};
template<typename T> struct adl_serializer<Magnum::Math::Vector2<T>> : adl_serializer<Magnum::Math::Vector<2, T>> {};

} // namespace nlohmann
