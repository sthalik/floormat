#pragma once

namespace floormat {

template<typename Signature> class function_view;

} // namespace floormat

namespace floormat::detail::function_view_impl {

template<bool Const, bool Noexcept, typename R, typename... Args> class function_view_base;

template<typename T> constexpr inline bool is_function_view = false;
template<typename Signature> constexpr inline bool is_function_view<floormat::function_view<Signature>> = true;

} // namespace floormat::detail::function_view_impl
