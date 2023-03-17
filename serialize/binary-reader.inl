#pragma once
#include "binary-reader.hpp"
#include "compat/exception.hpp"

namespace floormat::Serialize {

template<string_input_iterator It>
template<char_sequence Seq>
constexpr binary_reader<It>::binary_reader(const Seq& seq) noexcept
    : it{std::cbegin(seq)}, end{std::cend(seq)}
{}

template<string_input_iterator It>
constexpr binary_reader<It>::binary_reader(It begin, It end) noexcept :
    it{std::move(begin)}, end{std::move(end)}
{}

template<string_input_iterator It>
template<serializable T>
constexpr T binary_reader<It>::read() noexcept(false)
{
    constexpr std::size_t N = sizeof(T);
    fm_soft_assert((std::ptrdiff_t)N <= std::distance(it, end));
    num_bytes_read += N;
    char buf[N];
    for (auto i = 0_uz; i < N; i++)
        buf[i] = *it++;
    return maybe_byteswap(std::bit_cast<T, decltype(buf)>(buf));
}

template<string_input_iterator It>
template<std::size_t N>
constexpr std::array<char, N> binary_reader<It>::read() noexcept(false)
{
    std::array<char, N> array;
    if (std::is_constant_evaluated())
        array = {};
    fm_soft_assert(N <= (std::size_t)std::distance(it, end));
    num_bytes_read += N;
    for (auto i = 0_uz; i < N; i++)
        array[i] = *it++;
    return array;
}

template<string_input_iterator It>
constexpr void binary_reader<It>::assert_end() noexcept(false)
{
    fm_soft_assert(it == end);
}

template<string_input_iterator It, serializable T>
constexpr void operator>>(binary_reader<It>& reader, T& x) noexcept(false)
{
    x = reader.template read<T>();
}

template<string_input_iterator It, serializable T>
constexpr void operator<<(T& x, binary_reader<It>& reader) noexcept(false)
{
    x = reader.template read<T>();
}

template<string_input_iterator It>
template<std::size_t MAX>
constexpr auto binary_reader<It>::read_asciiz_string() noexcept(false)
{
    static_assert(MAX > 0);

    struct fixed_string final {
        char buf[MAX];
        std::size_t len;
        constexpr operator StringView() const noexcept { return { buf, len, StringViewFlag::NullTerminated }; }
    };

    fixed_string ret;
    for (auto i = 0_uz; i < MAX && it != end; i++)
    {
        const char c = *it++;
        ret.buf[i] = c;
        if (c == '\0')
        {
            ret.len = i;
            return ret;
        }
    }
    fm_throw("can't find string terminator"_cf);
}

template<string_input_iterator It>
constexpr char binary_reader<It>::peek() const
{
    fm_soft_assert(it != end);
    return *it;
}

} // namespace floormat::Serialize
