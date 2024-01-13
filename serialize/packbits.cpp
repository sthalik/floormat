#include "packbits.hpp"
#include "compat/assert.hpp"

namespace floormat {

using namespace floormat::detail_Pack;

namespace {

constexpr bool test1()
{
    constexpr size_t bits[] = { 5, 2, 1, 0 };
    constexpr size_t vals[] = {    3, 1, 0 };

    constexpr auto S0 = Storage<uint8_t,       8>{0b10111011uz};
    constexpr auto S1 = Storage<uint8_t, bits[0]>{0b00000101uz};
    constexpr auto S2 = Storage<uint8_t, bits[1]>{0b00000001uz};
    constexpr auto S3 = Storage<uint8_t, bits[2]>{0b00000000uz};

    using P0 = std::decay_t<decltype(S0)>;
    using P1 = P0::next<bits[0]>;
    using P2 = P1::next<bits[1]>;
    using P3 = P2::next<bits[2]>;

    static_assert(std::is_same_v<P1, Storage<uint8_t, vals[0]>>);
    static_assert(std::is_same_v<P2, Storage<uint8_t, vals[1]>>);
    static_assert(std::is_same_v<P3, Storage<uint8_t, vals[2]>>);

    static_assert(S0.advance<5>() == S1.value);
    static_assert(S1.advance<2>() == S2.value);
    static_assert(S2.advance<1>() == S3.value);

    static_assert(S0.get<bits[0]>() == (S0.value & (1<<bits[0])-1));
    static_assert(S1.get<bits[1]>() == (S1.value & (1<<bits[1])-1));
    static_assert(S2.get<bits[2]>() == (S2.value & (1<<bits[2])-1));

    return true;
}
static_assert(test1());

namespace test2 {
template<size_t Val> using ibits = Bits_<uint16_t, Val>;
using foo1 = ibits<2>;
using foo2 = ibits<10>;
using foo3 = ibits<4>;
using bar1 = check_size_overflow<uint16_t, 0, foo1, foo2>;
static_assert(bar1::result);
static_assert(bar1::size == 12);

using bar2 = check_size_overflow<uint16_t, 0, foo2>;
static_assert(bar2::result);
static_assert(bar2::size == 10);

using bar3 = check_size_overflow<uint16_t, 0, foo1, foo2, foo3>;
static_assert(bar3::result);
static_assert(bar3::size == 16);

using foo4 = ibits<1>;
using bar4 = check_size_overflow<uint16_t, 0, foo1, foo2, foo3, foo4>;
static_assert(!bar4::result);
static_assert(bar4::size == 17);

using foo5 = ibits<20>;
using bar5 = check_size_overflow<uint16_t, 0, foo1, foo2, foo3, foo4, foo5>;
static_assert(!bar5::result);
static_assert(bar5::size == 37);

using foo6 = ibits<40>;
using bar6 = check_size_overflow<uint16_t, 0, foo1, foo2, foo3, foo4, foo6>;
static_assert(!bar6::result);
static_assert(bar6::size == 57);
} // namespace test2

} // namespace

} // namespace floormat
