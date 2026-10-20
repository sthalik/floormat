#pragma once
#include "compat/move.hpp"
#include <cstddef>

namespace floormat {

// Library Fundamentals TS v2 std::experimental::propagate_const for raw pointers. MSVC's STL lacks it.
template<typename T>
class propagate_const final
{
    static_assert(std::is_pointer_v<T>);

    T t_ = nullptr;

public:
    using element_type = std::remove_pointer_t<T>;

    constexpr propagate_const() noexcept = default;
    constexpr propagate_const(propagate_const&&) noexcept = default;
    constexpr propagate_const& operator=(propagate_const&&) noexcept = default;
    propagate_const(const propagate_const&) = delete;
    propagate_const& operator=(const propagate_const&) = delete;

    template<typename U> requires std::is_convertible_v<U, T>
    constexpr propagate_const(U&& u) noexcept : t_{forward<U>(u)} {}

    template<typename U> requires std::is_convertible_v<U, T>
    constexpr propagate_const& operator=(U&& u) noexcept { t_ = forward<U>(u); return *this; }

    constexpr element_type* get() noexcept { return t_; }
    constexpr const element_type* get() const noexcept { return t_; }
    constexpr element_type* operator->() noexcept { return t_; }
    constexpr const element_type* operator->() const noexcept { return t_; }
    constexpr element_type& operator*() noexcept { return *t_; }
    constexpr const element_type& operator*() const noexcept { return *t_; }
    constexpr explicit operator bool() const noexcept { return t_ != nullptr; }

    friend constexpr bool operator==(const propagate_const& a, std::nullptr_t) noexcept { return a.t_ == nullptr; }
    template<typename U> requires std::is_pointer_v<U>
    friend constexpr bool operator==(const propagate_const& a, U u) noexcept { return a.t_ == u; }
};

} // namespace floormat
