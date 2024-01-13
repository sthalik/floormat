#include "packbits.hpp"
#include "compat/assert.hpp"

namespace floormat {

using namespace floormat::detail_Pack;

namespace {

constexpr bool test1()
{
    constexpr size_t left[] = { 8, 3, 2, 1, 0 };
    constexpr size_t rest[] = { 0, 5, 6, 7, 8 };
    constexpr size_t bits[] = {    5, 2, 1, 1 };
    constexpr size_t vals[] = {    8, 3, 1, 0 };
    constexpr auto S0 = Storage<uint8_t, left[0]>{0b10111011uz};
    constexpr auto S1 = Storage<uint8_t, left[1]>{0b00000101uz};
    constexpr auto S2 = Storage<uint8_t, left[2]>{0b00000010uz};
    constexpr auto S3 = Storage<uint8_t, left[3]>{0b00000001uz};
    constexpr auto S4 = Storage<uint8_t, left[4]>{0b00000000uz};
    static_assert(S0.Capacity == 8 - rest[0]);
    static_assert(S1.Capacity == 8 - rest[1]);
    static_assert(S2.Capacity == 8 - rest[2]);
    static_assert(S3.Capacity == 8 - rest[3]);
    static_assert(S4.Capacity == 8 - rest[4]);
    static_assert(S0.advance<left[0] - left[1]>() == S1.value);
    static_assert(S1.advance<left[1] - left[2]>() == S2.value);
    static_assert(S2.advance<left[2] - left[3]>() == S3.value);
    static_assert(S3.advance<left[3] - left[4]>() == S4.value);
    using P0 = std::decay_t<decltype(S0)>;
    using P1 = P0::next<bits[0]>;
    using P2 = P1::next<bits[1]>;
    using P3 = P2::next<bits[2]>;
    static_assert(P0::Capacity == vals[0]);
    static_assert(P1::Capacity == vals[1]);
    static_assert(P2::Capacity == vals[2]);
    static_assert(P3::Capacity == vals[3]);
    static_assert(std::is_same_v<P1, Storage<uint8_t, vals[1]>>);
    static_assert(std::is_same_v<P2, Storage<uint8_t, vals[2]>>);
    static_assert(std::is_same_v<P3, Storage<uint8_t, vals[3]>>);
    static_assert(std::is_same_v<P4, Storage<uint8_t, vals[4]>>);


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
