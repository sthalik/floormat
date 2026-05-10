#pragma once
#include "compat/move.hpp"

namespace floormat {

constexpr auto constantly(auto x)
noexcept(std::is_nothrow_move_constructible_v<decltype(x)>)
{
    return [x = move(x)]<typename... Ts> (const Ts&...) constexpr noexcept -> const auto& { return x; };
}

} // namespace floormat
