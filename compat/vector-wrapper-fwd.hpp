#pragma once

namespace floormat {

template<typename T> struct vector_wrapper;

enum class vector_wrapper_repr : uint8_t // todo! use this
{
    invalid,
    lvalue_reference_to_vector,
    const_reference_to_vector,
    vector,
    //rvalue_reference_to_vector,
};

} // namespace floormat
