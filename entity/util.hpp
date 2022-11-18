#pragma once
#include <type_traits>

namespace Corrade::Containers {
template<typename T> class BasicStringView;
using StringView = BasicStringView<const char>;
} // namespace Corrade::Containers

namespace floormat::entities {

template<typename T, typename = void> struct pass_by_value : std::bool_constant<std::is_fundamental_v<T>> {};
template<> struct pass_by_value<StringView> : std::true_type {};
template<typename T> struct pass_by_value<T, std::enable_if_t<std::is_trivially_copy_constructible_v<T> && sizeof(T) <= sizeof(void*)>> : std::true_type {};
template<typename T> constexpr inline bool pass_by_value_v = pass_by_value<T>::value;

template<typename T> using const_qualified = std::conditional_t<pass_by_value_v<T>, T, const T&>;
template<typename T> using ref_qualified = std::conditional_t<pass_by_value_v<T>, T, T&>;
template<typename T> using move_qualified = std::conditional_t<pass_by_value_v<T>, T, T&&>;

} // namespace floormat::entities
