#pragma once
#include "defs.hpp"
#include "move.hpp"

namespace floormat {

template<typename T>
union never_destroyed
{
    T value;

    template<typename... Args>
    explicit constexpr never_destroyed(Args&&... args) noexcept(noexcept(T(forward<Args>(args)...)));
    constexpr ~never_destroyed() noexcept;
    fm_DISABLE_MOVE_COPY(never_destroyed);
};

template<typename T>
template<typename... Args>
constexpr never_destroyed<T>::never_destroyed(Args&&... args) noexcept(noexcept(T(forward<Args>(args)...))):
    value(forward<Args>(args)...)
{
}

template<typename T>
constexpr never_destroyed<T>::~never_destroyed() noexcept
{
}

} // namespace floormat
