#pragma once
#include "magnum-vector2i.hpp"
#include <Magnum/Math/Vector.h>
#include <nlohmann/json.hpp>

namespace nlohmann {

template<size_t N, typename T>
struct adl_serializer<Magnum::Math::Vector<N, T>> {
    using vec = Magnum::Math::Vector<N, T>;
    static void to_json(json& j, const vec& val);
    static void from_json(const json& j, vec& val);
};

template <size_t N, typename T>
void adl_serializer<Magnum::Math::Vector<N, T>>::to_json(json& j, const vec& val)
{
    std::array<T, N> array{};
    for (auto i = 0uz; i < N; i++)
        array[i] = val[i];
    using nlohmann::to_json;
    to_json(j, array);
}

template <size_t N, typename T>
void adl_serializer<Magnum::Math::Vector<N, T>>::from_json(const json& j, vec& val)
{
    std::array<T, N> array{};
    using nlohmann::from_json;
    from_json(j, array);
    for (auto i = 0uz; i < N; i++)
        val[i] = array[i];
}

template<typename T> struct adl_serializer<Magnum::Math::Vector2<T>> : adl_serializer<Magnum::Math::Vector<2, T>> {};
template<typename T> struct adl_serializer<Magnum::Math::Vector3<T>> : adl_serializer<Magnum::Math::Vector<3, T>> {};
template<typename T> struct adl_serializer<Magnum::Math::Vector4<T>> : adl_serializer<Magnum::Math::Vector<4, T>> {};
template<typename T> struct adl_serializer<Magnum::Math::Color3<T>>  : adl_serializer<Magnum::Math::Vector<3, T>> {};
template<typename T> struct adl_serializer<Magnum::Math::Color4<T>>  : adl_serializer<Magnum::Math::Vector<4, T>> {};

} // namespace nlohmann
