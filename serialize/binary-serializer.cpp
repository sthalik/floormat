#include "binary-reader.inl"
#include "binary-writer.inl"
#include <array>

namespace floormat::Serialize {

#if 0
template<std::size_t N>
struct byte_array_iterator final
{
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = char;
    using pointer           = char*;
    using reference         = char&;

    constexpr byte_array_iterator(char (&buf)[N]) : buf{&buf}, i{0} {}
    constexpr byte_array_iterator(char (&buf)[N], std::size_t i) : buf{&buf}, i{i} {}

    constexpr reference operator*() const { fm_assert(i < N); return (*buf)[i]; }
    constexpr pointer operator->() { fm_assert(i < N); return &(*buf)[i]; }
    constexpr byte_array_iterator<N>& operator++() noexcept { i++; return *this; }
    constexpr byte_array_iterator<N> operator++(int) noexcept { byte_array_iterator<N> tmp = *this; ++(*this); return tmp; }
    friend constexpr bool operator==(const byte_array_iterator<N>&& a, const byte_array_iterator<N>&& b) noexcept = default;

private:
    char (*buf)[N];
    std::size_t i;
};

template struct byte_array_iterator<sizeof(double)>;
#endif

[[maybe_unused]]
static constexpr bool test1()
{
    constexpr std::array<char, 4> bytes = { 1, 0, 1, 0 };
    auto x = binary_reader(bytes.cbegin(), bytes.cend());
    return x.read_u<unsigned char>().bytes[0] == 1 &&
           x.read_u<unsigned char>().bytes[0] == 0 &&
           x.read_u<unsigned char>().bytes[0] == 1 &&
           x.read_u<unsigned char>().bytes[0] == 0;
}
static_assert(test1());

[[maybe_unused]]
static constexpr bool test2()
{
    constexpr std::array<char, 4> bytes = { 1, 0, 1, 0 };
    auto r = binary_reader(bytes.cbegin(), bytes.cend());
    const auto x = r.read_u<int>();
    r.assert_end();
    return x.bytes[0] == 1 && x.bytes[1] == 0 && x.bytes[2] == 1 && x.bytes[3] == 0;
}
static_assert(test2());

using test3 = binary_reader<std::array<char, 1>::iterator>;
static_assert(std::is_same_v<test3&, decltype( std::declval<test3&>() >> std::declval<int&>() )>);

using test4 = binary_writer<std::array<char, sizeof(int)>::iterator>;
static_assert(std::is_same_v<test4&, decltype( std::declval<test4&>() << int() )>);

[[maybe_unused]]
static constexpr bool test5()
{
    std::array<char, 4> bytes = {};
    auto w = binary_writer(bytes.begin());
    w << (char)0;
    w << (char)1;
    w << (char)2;
    w << (char)3;
    return bytes[0] == 0 && bytes[1] == 1 && bytes[2] == 2 && bytes[3] == 3;
}
static_assert(test5());

} // namespace floormat::Serialize
