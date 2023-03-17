#include "binary-reader.inl"
#include "binary-writer.inl"
#include <array>

namespace floormat::Serialize {

namespace {

[[maybe_unused]]
constexpr bool test1()
{
    constexpr std::array<char, 4> bytes = { 1, 2, 3, 4 };
    auto x = binary_reader(bytes.cbegin(), bytes.cend());
    return x.read<std::uint32_t>() == 67305985;
}
static_assert(test1());

[[maybe_unused]]
constexpr bool test2()
{
    constexpr std::array<char, 4> bytes = { 4, 3, 2, 1 };
    auto r = binary_reader(bytes.cbegin(), bytes.cend());
    const auto x = r.read<std::uint32_t>();
    r.assert_end();
    return x == 16909060;
}
static_assert(test2());

using test3 = binary_reader<std::array<char, 1>::iterator>;
static_assert(std::is_same_v<void, decltype( std::declval<test3&>() >> std::declval<int&>() )>);

using test4 = binary_writer<std::array<char, sizeof(int)>::iterator>;
static_assert(std::is_same_v<test4&, decltype( std::declval<test4&>() << int() )>);

[[maybe_unused]]
constexpr bool test5()
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

[[maybe_unused]]
constexpr bool test6()
{
    std::array<char, 5> bytes = {
        'f', 'o', 'o', '\0', 42,
    };
    auto r = binary_reader(bytes.cbegin(), bytes.cend());
    fm_assert(r.read_asciiz_string<4>() == "foo"_s);
    unsigned char b = 0;
    b << r;
    fm_assert(b == 42);
    r.assert_end();
    return true;
}
static_assert(test6());

} // namespace

} // namespace floormat::Serialize
