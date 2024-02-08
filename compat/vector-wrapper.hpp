#pragma once
#include "vector-wrapper-fwd.hpp" // todo!
#include <vector>

namespace floormat {

template<typename T>
struct vector_wrapper final
{
    using vector_type = std::conditional_t<std::is_const_v<T>, const std::vector<std::remove_const_t<T>>, std::vector<T>>;
    using qualified_type = std::conditional_t<std::is_const_v<T>, vector_type, vector_type&>;

    qualified_type vec;
};

} // namespace floormat
