#pragma once
#include "binary-writer.hpp"
#include "binary-serializer.hpp"
#include "compat/assert.hpp"
#include <type_traits>
#include <Corrade/Containers/StringView.h>

namespace floormat::Serialize {

template<std::output_iterator<char> It>
constexpr binary_writer<It>::binary_writer(It it) noexcept : it{it}, _bytes_written{0} {}

template<std::output_iterator<char> It>
template<integer T>
constexpr void binary_writer<It>::write(T x) noexcept
{
    union {
        T datum;
        char bytes[sizeof(T)];
    } buf;

    if (std::is_constant_evaluated())
        for (std::size_t i = 0; i < std::size(buf.bytes); i++)
            buf.bytes[i] = 0;
    _bytes_written += sizeof(T);
    if constexpr(sizeof(T) == 1)
        buf.bytes[0] = (char)x;
    else if (!std::is_constant_evaluated())
        buf.datum = maybe_byteswap(x);
    else
        for (std::size_t i = 0; i < sizeof(T); x >>= 8, i++)
            buf.bytes[i] = (char)(unsigned char)x;
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
    _bytes_written += sizeof(T);
    buf.datum = maybe_byteswap(x);
    for (std::size_t i = 0; i < sizeof(T); i++)
        *it++ = buf.bytes[i];
}

template<std::output_iterator<char> It, serializable T>
constexpr binary_writer<It>& operator<<(binary_writer<It>& writer, T x) noexcept
{
    writer.template write<T>(x);
    return writer;
}

template<std::output_iterator<char> It>
constexpr void binary_writer<It>::write_asciiz_string(StringView str) noexcept
{
    fm_debug_assert(str.flags() & StringViewFlag::NullTerminated);
    const auto sz = str.size();
    _bytes_written += sz + 1;
    for (std::size_t i = 0; i < sz; i++)
        *it++ = str[i];
    *it++ = '\0';
}

} // namespace floormat::Serialize
