#pragma once

namespace floormat::Pack_impl {

[[noreturn]] void throw_on_read_nonzero() noexcept(false);
[[noreturn]] void throw_on_write_input_bit_overflow() noexcept(false);

template<size_t... Ns> struct expand_sum;
template<size_t N, size_t... Ns> struct expand_sum<N, Ns...> { static constexpr size_t value = N + expand_sum<Ns...>::value; };
template<> struct expand_sum<> { static constexpr size_t value = 0; };
template<size_t... Ns> constexpr inline size_t sum = expand_sum<Ns...>::value;

} // namespace floormat::Pack_impl
