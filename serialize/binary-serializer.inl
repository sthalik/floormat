#pragma once

#include "binary-serializer.hpp"

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
constexpr binary_reader<It>::~binary_reader() noexcept
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
    fm_assert(std::distance(it, end) >= (std::ptrdiff_t) sizeof(T));
    if constexpr(std::endian::native == std::endian::big)
        for (int i = sizeof(T) - 1; i >= 0; i--)
            buf.bytes[i] = *it++;
    else
        for (std::size_t i = 0; i < sizeof(T); i++)
            buf.bytes[i] = *it++;
    return buf;
}

template<integer T>
constexpr inline T maybe_byteswap(T x)
{
    if constexpr(std::endian::native == std::endian::big)
        return std::byteswap(x);
    else
        return x;
}

template<std::output_iterator<char> It>
constexpr binary_writer<It>::binary_writer(It it) noexcept : it{it} {}

template<std::output_iterator<char> It>
template<integer T>
void binary_writer<It>::write(T x) noexcept
{
    union {
        T datum;
        char bytes[sizeof(T)];
    } buf;
    buf.datum = maybe_byteswap(x);
    for (std::size_t i = 0; i < sizeof(T); i++)
        *it++ = buf.bytes[i];
}

template<std::output_iterator<char> It>
template<std::floating_point T>
void binary_writer<It>::write(T x) noexcept
{
    union {
        T datum;
        char bytes[sizeof(T)];
    } buf;
    buf.datum = maybe_byteswap(x);
    for (std::size_t i = 0; i < sizeof(T); i++)
        *it++ = buf.bytes[i];
}

template<string_input_iterator It, serializable T>
binary_reader<It>& operator>>(binary_reader<It>& reader, T& x) noexcept
{
    value_u u = reader.template read<T>();
    x = *reinterpret_cast<T*>(&u.bytes[0]);
    return reader;
}

template<std::output_iterator<char> It, serializable T>
binary_writer<It>& operator<<(binary_writer<It>& writer, T x) noexcept
{
    writer.template write<T>(x);
    return writer;
}

} // namespace floormat::Serialize


