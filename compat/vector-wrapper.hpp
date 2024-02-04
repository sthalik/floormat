#pragma once
#include <vector>

namespace floormat {

template<typename T>
struct vector_wrapper final
{
    using vector_type = std::conditional_t<std::is_const_v<T>,
                                           const std::vector<std::remove_const_t<T>>,
                                           std::vector<T>>;
    vector_type& vec;
};

} // namespace floormat
