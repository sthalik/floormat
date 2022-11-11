#pragma once
#include <type_traits>

namespace floormat {

template<typename F>
class emplacer {
    F fun;
    using type = std::decay_t<decltype(std::declval<F>()())>;

public:
    explicit constexpr emplacer(F&& fun) noexcept : fun{std::forward<F>(fun)} {}
    constexpr operator type() const noexcept { return fun(); }
};

} // namespace floormat
