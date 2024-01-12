#include "packbits.hpp"

namespace floormat {

using namespace floormat::detail_Pack;

namespace {

constexpr bool test1()
{
    using S1 = Storage<uint8_t, 8>;
    S1 st1{0b10111011};
    fm_assert(st1.value == 0b10111011);

    fm_assert(st1.get<3>() == 0b011);
    using S2 = typename S1::next<3>;
    S2 st2{st1.advance<3>()};
    static_assert(std::is_same_v<S2, Storage<uint8_t, 5>>);
    fm_assert(st2.value == 0b10111);

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
