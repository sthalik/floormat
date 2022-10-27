#include "binary-serializer.inl"
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

struct value_buf final {
    value_u value;
    std::int8_t len;

    explicit constexpr value_buf(value_u value, std::int8_t len) : value{value}, len{len} {}
    constexpr bool operator==(const value_buf& o) const noexcept;

    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(value_buf);
    fm_DECLARE_DEFAULT_COPY_ASSIGNMENT(value_buf);
};

constexpr bool value_buf::operator==(const value_buf& o) const noexcept
{
    const auto N = len;
    if (N != o.len)
        return false;
    for (std::int8_t i = 0; i < N; i++)
        if (value.bytes[i] != o.value.bytes[i])
            return false;
    return true;
}

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
    return x.bytes[0] == 1 && x.bytes[1] == 0 && x.bytes[2] == 1 && x.bytes[3] == 0;
}
static_assert(test2());

template<typename T>
[[maybe_unused]] static constexpr T maybe_byteswap(T x)
{
    if constexpr(std::endian::native == std::endian::big)
        return std::byteswap(x);
    else
        return x;
}

} // namespace floormat::Serialize
