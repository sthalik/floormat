#pragma once

namespace floormat {

enum class vector_wrapper_repr : uint8_t
{
    invalid,
    lvalue_reference_to_vector,
    const_reference_to_vector,
    vector,
};

template<typename T, vector_wrapper_repr> struct vector_wrapper;

} // namespace floormat
