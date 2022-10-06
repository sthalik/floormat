#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector.h>
#include <nlohmann/json.hpp>

namespace nlohmann {

template<std::size_t N, typename T>
struct adl_serializer<Magnum::Math::Vector<N, T>> final {
    static void to_json(json& j, const Magnum::Math::Vector2<T>& val)
    {
        std::array<T, N> array{};
        for (std::size_t i; i < std::size(val); i++)
            array[i] = val[i];
        j = array;
    }
    static void from_json(const json& j, Magnum::Math::Vector2<T>& val)
    {
        std::array<T, N> array = j;
        for (std::size_t i; i < std::size(val); i++)
            val[i] = array[i];
    }
};

} // namespace nlohmann
