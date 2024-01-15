#include "packbits-read.hpp"
#include "compat/assert.hpp"

namespace floormat {

using namespace floormat::detail_Pack_input;

namespace {

template<std::unsigned_integral T, size_t N> constexpr inline T lowbits = (T{1} << N)-T{1};

static_assert(!input<uint32_t, 3>{65535}.check_zero());
static_assert(input<uint32_t, 30>{65535}.advance<16>() == 0);

static_assert(input<uint32_t, 30>::next<16>{ input<uint32_t, 30>{65535}.advance<16>() }.check_zero());
static_assert(input<uint32_t, 30>::next<16>{}.Capacity == 14);

constexpr bool test1()
{
    constexpr size_t bits[] = { 5, 2, 1 };
    constexpr size_t vals[] = { 8, 3, 1, 0 };

    constexpr auto S0 = input<uint8_t, vals[0]>{0b10111011};
    constexpr auto S1 = input<uint8_t, vals[1]>{0b00000101};
    constexpr auto S2 = input<uint8_t, vals[2]>{0b00000001};
    constexpr auto S3 = input<uint8_t, vals[3]>{0b00000000};

    using P0 = std::decay_t<decltype(S0)>;
    using P1 = P0::next<bits[0]>;
    using P2 = P1::next<bits[1]>;
    using P3 = P2::next<bits[2]>;

    static_assert(std::is_same_v<P0, input<uint8_t, vals[0]>>);
    static_assert(std::is_same_v<P1, input<uint8_t, vals[1]>>);
    static_assert(std::is_same_v<P2, input<uint8_t, vals[2]>>);
    static_assert(std::is_same_v<P3, input<uint8_t, vals[3]>>);

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

constexpr bool test3()
{
    constexpr auto S0 = input<uint16_t, 16>{0b1110100110001011};
    constexpr auto S1 = input<uint16_t,  4>{0b1110};
    constexpr auto S2 = input<uint16_t,  1>{0b1};

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
    using Tuple_u32 = make_tuple_type<uint32_t, 3>;
    static_assert(std::is_same_v<Tuple_u32, std::tuple<uint32_t, uint32_t, uint32_t>>);
    using Tuple_u8  = make_tuple_type<uint8_t, 3>;
    {
        Tuple_u32 tuple{};
        static_assert(lowbits<uint32_t, 17> == 0x1ffffU);
        read_(tuple, input<uint32_t, 32>{(uint32_t)-1}, std::make_index_sequence<3>{}, make_pack<uint32_t, 17, 14, 1>{});
        auto [a, b, c] = tuple;
        fm_assert(a == lowbits<uint32_t, 17>);
        fm_assert(b == lowbits<uint32_t, 14>);
        fm_assert(c & 1);
    }
    {
        Tuple_u8 tuple{};
        read_(tuple, input<uint8_t, 8>{0b101011}, std::make_index_sequence<3>{}, make_pack<uint8_t, 1, 3, 2>{});
        auto [a, b, c] = tuple;
        fm_assert(a == 0b1);
        fm_assert(b == 0b101);
        fm_assert(c == 0b10);
    }
    {
        std::tuple<> empty_tuple;
        read_(empty_tuple, input<uint8_t, 8>{0}, std::index_sequence<>{}, make_pack<uint8_t>{});
        Tuple_u8 tuple{}; (void)tuple;
        //read_(empty_tuple, input<uint8_t, 8>{1}, std::index_sequence<>{}, make_tuple<uint8_t>{});
        //read_(tuple, input<uint8_t, 5>{0b11111}, std::make_index_sequence<3>{}, make_tuple<uint8_t, 2, 2, 2>{});
        //(void)input<uint8_t, 9>{};
        //read_(empty_tuple, input<uint8_t, 8>{}, std::index_sequence<0>{}, make_tuple<uint8_t, 1>{});
        //read_(empty_tuple, input<uint8_t, 8>{1}, std::index_sequence<>{}, make_tuple<uint8_t, 1>{});
    }

    return true;
}
static_assert(test4());

} // namespace

} // namespace floormat
