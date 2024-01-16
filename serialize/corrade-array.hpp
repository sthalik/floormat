#pragma once
#include "compat/exception.hpp"
#include <Corrade/Containers/Array.h>
#include <nlohmann/json.hpp>

namespace nlohmann {

template<typename T, typename D>
struct adl_serializer<Corrade::Containers::Array<T, D>>
{
    static void to_json(json& j, const Corrade::Containers::Array<T, D>& array)
    {
        j.clear();
        for (const T& x : array)
            j.push_back(x);
    }

    static void from_json(const json& j, Corrade::Containers::Array<T>& array)
    {
        fm_soft_assert(j.is_array());
        auto size = (uint32_t)j.size();
        array = Corrade::Containers::Array<T>{size};
        for (uint32_t i = 0; i < size; i++)
            array[i] = j[i];
    }
};

} // namespace nlohmann
