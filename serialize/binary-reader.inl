#pragma once
#include "binary-reader.hpp"
#include "compat/assert.hpp"

namespace floormat::Serialize {

template<string_input_iterator It>
template<char_sequence Seq>
constexpr binary_reader<It>::binary_reader(const Seq& seq) noexcept
        : it{std::begin(seq)}, end{std::end(seq)}
{}

template<string_input_iterator It>
constexpr binary_reader<It>::binary_reader(It begin, It end) noexcept :
        it{begin}, end{end}
{}

template<string_input_iterator It>
template<std::floating_point T>
constexpr value_u binary_reader<It>::read_u() noexcept
{
    value_u buf;
    static_assert(sizeof(T) <= sizeof(buf));
    fm_assert(std::distance(it, end) >= sizeof(T));
    num_bytes_read += sizeof(T);
    for (int i = 0; i < sizeof(T); i++)
        buf.bytes[i] = *it++;
    return buf;
}

template<string_input_iterator It>
template<typename T>
T binary_reader<It>::read() noexcept
{
    value_u buf = read_u<T>();
    return *reinterpret_cast<T>(buf.bytes);
}

template<string_input_iterator It>
template<std::size_t N>
constexpr std::array<char, N> binary_reader<It>::read() noexcept
{
    std::array<char, N> array;
    if (std::is_constant_evaluated())
        array = {};
    fm_assert(N <= (std::size_t)std::distance(it, end));
    num_bytes_read += N;
    for (std::size_t i = 0; i < N; i++)
        array[i] = *it++;
    return array;
}

template<string_input_iterator It>
constexpr void binary_reader<It>::assert_end() noexcept
{
    fm_assert(it == end);
}

template<string_input_iterator It>
template<integer T>
constexpr value_u binary_reader<It>::read_u() noexcept
{
    value_u buf;
    if (std::is_constant_evaluated())
        for (std::size_t i = 0; i < std::size(buf.bytes); i++)
            buf.bytes[i] = 0;
    static_assert(sizeof(T) <= sizeof(buf));
    fm_assert((std::ptrdiff_t)sizeof(T) <= std::distance(it, end));
    num_bytes_read += sizeof(T);
    if constexpr(std::endian::native == std::endian::big)
        for (int i = sizeof(T) - 1; i >= 0; i--)
            buf.bytes[i] = *it++;
    else
        for (std::size_t i = 0; i < sizeof(T); i++)
            buf.bytes[i] = *it++;
    return buf;
}

template<string_input_iterator It, serializable T>
binary_reader<It>& operator>>(binary_reader<It>& reader, T& x) noexcept
{
    value_u u = reader.template read<T>();
    x = *reinterpret_cast<T*>(&u.bytes[0]);
    return reader;
}

} // namespace floormat::Serialize
