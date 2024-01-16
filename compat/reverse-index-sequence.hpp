#pragma once
#include <utility>

namespace floormat {

template <size_t... Is>
constexpr std::index_sequence<sizeof...(Is)-1u-Is...> reverse_index_sequence(std::index_sequence<Is...>);
template <size_t N>
using make_reverse_index_sequence = decltype(reverse_index_sequence(std::make_index_sequence<N>{}));

} // namespace floormat
