#include "packbits.hpp"
#include "compat/assert.hpp"

namespace floormat {

using namespace floormat::detail_Pack;

namespace {

template<std::unsigned_integral T, size_t N> constexpr inline T lowbits = (T{1} << N)-T{1};

template<size_t Val> using us_bits = Bits_<uint16_t, Val>;

static_assert(!Storage<uint32_t, 3>{65535}.check_zero());
static_assert(Storage<uint32_t, 30>{65535}.advance<16>() == 0);

static_assert(Storage<uint32_t, 30>::next<16>{
    Storage<uint32_t, 30>{65535}.advance<16>()
}.check_zero());
static_assert(Storage<uint32_t, 30>::next<16>{}.Capacity == 14);

constexpr bool test1()
{
    constexpr size_t bits[] = { 5, 2, 1 };
    constexpr size_t vals[] = { 8, 3, 1, 0 };

    constexpr auto S0 = Storage<uint8_t, vals[0]>{0b10111011};
    constexpr auto S1 = Storage<uint8_t, vals[1]>{0b00000101};
    constexpr auto S2 = Storage<uint8_t, vals[2]>{0b00000001};
    constexpr auto S3 = Storage<uint8_t, vals[3]>{0b00000000};

    using P0 = std::decay_t<decltype(S0)>;
    using P1 = P0::next<bits[0]>;
    using P2 = P1::next<bits[1]>;
    using P3 = P2::next<bits[2]>;

    static_assert(std::is_same_v<P0, Storage<uint8_t, vals[0]>>);
    static_assert(std::is_same_v<P1, Storage<uint8_t, vals[1]>>);
    static_assert(std::is_same_v<P2, Storage<uint8_t, vals[2]>>);
    static_assert(std::is_same_v<P3, Storage<uint8_t, vals[3]>>);

    static_assert(S0.advance<      0>() == S0.value);
    static_assert(S0.advance<bits[0]>() == S1.value);
    static_assert(S1.advance<bits[1]>() == S2.value);
    static_assert(S2.advance<bits[2]>() == S3.value);

    static_assert(S0.get<bits[0]>() == (S0.value & (1<<bits[0])-1));
    static_assert(S1.get<bits[1]>() == (S1.value & (1<<bits[1])-1));
    static_assert(S2.get<bits[2]>() == (S2.value & (1<<bits[2])-1));
    static_assert(S3.check_zero());

    return true;
}
static_assert(test1());

constexpr bool test2()
{
    using foo1 = us_bits<2>;
    using foo2 = us_bits<10>;
    using foo3 = us_bits<4>;
    using bar1 = check_size_overflow<uint16_t, 0, foo1, foo2>;
    static_assert(bar1::result);
    static_assert(bar1::size == 12);

    using bar2 = check_size_overflow<uint16_t, 0, foo2>;
    static_assert(bar2::result);
    static_assert(bar2::size == 10);

    using bar3 = check_size_overflow<uint16_t, 0, foo1, foo2, foo3>;
    static_assert(bar3::result);
    static_assert(bar3::size == 16);

    using foo4 = us_bits<1>;
    using bar4 = check_size_overflow<uint16_t, 0, foo1, foo2, foo3, foo4>;
    static_assert(!bar4::result);
    static_assert(bar4::size == 17);

    using foo5 = us_bits<20>;
    using bar5 = check_size_overflow<uint16_t, 0, foo1, foo2, foo3, foo4, foo5>;
    static_assert(!bar5::result);
    static_assert(bar5::size == 37);

    using foo6 = us_bits<40>;
    using bar6 = check_size_overflow<uint16_t, 0, foo1, foo2, foo3, foo4, foo6>;
    static_assert(!bar6::result);
    static_assert(bar6::size == 57);

    return true;
}
static_assert(test2());

constexpr bool test3()
{
    constexpr auto S0 = Storage<uint16_t, 16>{0b1110100110001011};
    constexpr auto S1 = Storage<uint16_t,  4>{0b1110};
    constexpr auto S2 = Storage<uint16_t,  1>{0b1};

    static_assert(S0.get<12>() == 0b100110001011);
    static_assert(S0.advance<12>() == S1.value);

    static_assert(S1.get<3>() == 0b110);
    static_assert(S1.advance<3>() == S2.value);

    static_assert(S2.get<1>() == 0b1);
    static_assert(S2.advance<1>() == 0);

    return true;
}
static_assert(test3());

static_assert(std::is_same_v< make_tuple_type<uint8_t, 3>,  std::tuple<uint8_t, uint8_t, uint8_t> >);

constexpr bool test4()
{
    using Tuple = std::tuple<uint32_t, uint32_t, uint32_t>;
    Tuple tuple{};
    assign_tuple2(tuple, Storage<uint32_t, 32>{(uint32_t)-1}, std::make_index_sequence<3>{},
                  Bits_<uint32_t, 17>{}, Bits_<uint32_t, 14>{}, Bits_<uint32_t, 1>{});
    auto [a, b, c] = tuple;

    static_assert(lowbits<uint32_t, 17> != 0);
    fm_assert(a == lowbits<uint32_t, 17>);
    fm_assert(b == lowbits<uint32_t, 14>);
    fm_assert(c & 1);

    //Assign::do_tuple(tuple, Storage<uint32_t, 32>{(uint32_t)-1});

    return true;
}
static_assert(test4());

constexpr bool test5()
{
    auto st = Storage<uint32_t, 32>{0xB16B00B5};
    uint32_t a, b, c;
    using Tuple = std::tuple<uint32_t&, uint32_t&, uint32_t&>;
    auto t = Tuple{a, b, c};
    //assign_tuple<uint32_t, std::make_index_sequence<3>, Tuple,

    return true;
}
static_assert(test4());

} // namespace

} // namespace floormat
