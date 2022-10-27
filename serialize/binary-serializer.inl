#pragma once

#include "binary-serializer.hpp"

namespace floormat::Serialize {

template<std::forward_iterator It>
template<std::floating_point T>
constexpr value_u binary_reader<It>::read_u() noexcept
{
    value_u buf;
    static_assert(sizeof(T) <= sizeof(buf));
    fm_assert(std::distance(it, end) >= sizeof(T));
    for (int i = 0; i < sizeof(T); i++)
        buf.bytes[i] = *it++;
    return buf;
}

template<std::forward_iterator It>
template<typename T>
T binary_reader<It>::read() noexcept
{
    value_u buf = read_u<T>();
    return *reinterpret_cast<T>(buf.bytes);
}

template<std::forward_iterator It>
constexpr binary_reader<It>::~binary_reader() noexcept
{
    fm_assert(it == end);
}

template<std::forward_iterator It>
template<integer T>
constexpr value_u binary_reader<It>::read_u() noexcept
{
    value_u buf;
    if (std::is_constant_evaluated())
        for (std::size_t i = 0; i < std::size(buf.bytes); i++)
            buf.bytes[i] = 0;
    static_assert(sizeof(T) <= sizeof(buf));
    fm_assert(std::distance(it, end) >= (std::ptrdiff_t) sizeof(T));
    if constexpr(std::endian::native == std::endian::big)
        for (int i = sizeof(T) - 1; i >= 0; i--)
            buf.bytes[i] = *it++;
    else
        for (std::size_t i = 0; i < sizeof(T); i++)
            buf.bytes[i] = *it++;
    return buf;
}

} // namespace floormat::Serialize


