#pragma once
#include <memory>

namespace floormat {

template<typename T>
struct shared_ptr_wrapper final
{
    std::shared_ptr<T> ptr;
};

} // namespace floormat
