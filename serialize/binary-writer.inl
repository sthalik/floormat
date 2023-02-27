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
template<serializable T>
constexpr void binary_writer<It>::write(T x) noexcept
{
    _bytes_written += sizeof(T);
    constexpr std::size_t N = sizeof(T);
    const auto buf = std::bit_cast<std::array<char, N>, T>(maybe_byteswap(x));
    for (auto i = 0_uz; i < N; i++)
        *it++ = buf[i];
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
    for (auto i = 0_uz; i < sz; i++)
        *it++ = str[i];
    *it++ = '\0';
}

} // namespace floormat::Serialize
