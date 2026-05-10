#pragma once
#include <concepts>

namespace floormat {

template<typename T, typename U> // todo move to compat
concept Qualified = requires () {
    requires std::same_as<U, std::remove_cvref_t<U>> &&
             std::same_as<std::remove_cvref_t<T>, U>;
};

} // namespace floormat
