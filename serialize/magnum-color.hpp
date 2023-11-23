#pragma once
#include "magnum-vector.hpp"
#include <Magnum/Math/Math.h>
#include <nlohmann/json_fwd.hpp>

namespace nlohmann {

template<> struct adl_serializer<Magnum::Color3> : nlohmann::adl_serializer<Magnum::Math::Vector<3, float>> {};
template<> struct adl_serializer<Magnum::Color4> : nlohmann::adl_serializer<Magnum::Math::Vector<4, float>> {};

template<> struct adl_serializer<Magnum::Color3ub> : nlohmann::adl_serializer<Magnum::Math::Vector<3, std::uint8_t>> {};
template<> struct adl_serializer<Magnum::Color4ub> : nlohmann::adl_serializer<Magnum::Math::Vector<4, std::uint8_t>> {};

} // namespace nlohmann
