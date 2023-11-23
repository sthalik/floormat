#pragma once
#include <nlohmann/json_fwd.hpp>

namespace nlohmann {

template<floormat::size_t N, typename T>
struct adl_serializer<Magnum::Math::Vector<N, T>>
{
    static void to_json(json& j, Magnum::Math::Vector<N, T> val);
    static void from_json(const json& j, Magnum::Math::Vector<N, T>& val);
};

template<typename T> struct adl_serializer<Magnum::Math::Vector2<T>> : adl_serializer<Magnum::Math::Vector<2, T>> {};
template<typename T> struct adl_serializer<Magnum::Math::Vector3<T>> : adl_serializer<Magnum::Math::Vector<3, T>> {};
template<typename T> struct adl_serializer<Magnum::Math::Vector4<T>> : adl_serializer<Magnum::Math::Vector<4, T>> {};

template<> struct adl_serializer<Magnum::Color3>   : nlohmann::adl_serializer<Magnum::Math::Vector<3, float>> {};
template<> struct adl_serializer<Magnum::Color4>   : nlohmann::adl_serializer<Magnum::Math::Vector<4, float>> {};
template<> struct adl_serializer<Magnum::Color3ub> : nlohmann::adl_serializer<Magnum::Math::Vector<3, floormat::uint8_t>> {};
template<> struct adl_serializer<Magnum::Color4ub> : nlohmann::adl_serializer<Magnum::Math::Vector<4, floormat::uint8_t>> {};

} // namespace nlohmann
