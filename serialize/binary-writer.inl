#pragma once
#include "binary-writer.hpp"
#include "binary-serializer.hpp"
#include "compat/assert.hpp"
#include <type_traits>
#include <Corrade/Containers/StringView.h>

namespace floormat::Serialize {

template<std::output_iterator<char> It>
constexpr binary_writer<It>::binary_writer(It it, size_t bytes_allocated) noexcept :
      it{it},
      _bytes_written{0},
      _bytes_allocated{bytes_allocated}
{}

template<std::output_iterator<char> It>
template<serializable T>
constexpr void binary_writer<It>::write(T x) noexcept
{
    constexpr size_t N = sizeof(T);
    _bytes_written += N;
    fm_assert(_bytes_written <= _bytes_allocated);
    const auto buf = std::bit_cast<std::array<char, N>, T>(maybe_byteswap(x));
    for (auto i = 0uz; i < N; i++)
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
    //fm_debug_assert(str.flags() & StringViewFlag::NullTerminated);
    fm_assert(!str.find('\0'));
    const auto sz = str.size();
    _bytes_written += sz + 1;
    fm_assert(_bytes_written <= _bytes_allocated);
    for (auto i = 0uz; i < sz; i++)
        *it++ = str[i];
    *it++ = '\0';
}

} // namespace floormat::Serialize
