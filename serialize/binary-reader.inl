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
template<serializable T>
constexpr T binary_reader<It>::read() noexcept
{
    constexpr std::size_t N = sizeof(T);
    fm_assert((std::ptrdiff_t)N <= std::distance(it, end));
    num_bytes_read += N;
    char buf[N];
    for (std::size_t i = 0; i < N; i++)
        buf[i] = *it++;
    return maybe_byteswap(std::bit_cast<T, decltype(buf)>(buf));
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

template<string_input_iterator It, serializable T>
binary_reader<It>& operator>>(binary_reader<It>& reader, T& x) noexcept
{
    x = reader.template read<T>();
    return reader;
}

template<string_input_iterator It>
template<std::size_t MAX>
constexpr auto binary_reader<It>::read_asciiz_string() noexcept
{
    static_assert(MAX > 0);

    struct fixed_string final {
        char buf[MAX];
        std::size_t len;
    };

    fixed_string ret;
    for (std::size_t i = 0; i < MAX && it != end; i++)
    {
        const char c = *it++;
        ret.buf[i] = c;
        if (c == '\0')
        {
            ret.len = i;
            return ret;
        }
    }
    fm_abort("can't find string terminator");
}

} // namespace floormat::Serialize
