#pragma once
#include "vector-wrapper-fwd.hpp"
#include <vector>

namespace floormat {

template<typename T, vector_wrapper_repr> struct vector_wrapper_traits;

template<typename T> struct vector_wrapper_traits<T, vector_wrapper_repr::ref> { using vector_type = std::vector<T>&; };
template<typename T> struct vector_wrapper_traits<T, vector_wrapper_repr::const_ref> { using vector_type = const std::vector<T>&; };
template<typename T> struct vector_wrapper_traits<T, vector_wrapper_repr::value> { using vector_type = std::vector<T>; };

template<typename T, vector_wrapper_repr R>
struct vector_wrapper final
{
    typename vector_wrapper_traits<T, R>::vector_type vec;
};

} // namespace floormat
