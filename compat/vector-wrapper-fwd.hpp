#pragma once

namespace floormat {

enum class vector_wrapper_repr : uint8_t
{
    invalid,
    ref,
    const_ref,
    value,
};

template<typename T, vector_wrapper_repr> struct vector_wrapper;

} // namespace floormat
