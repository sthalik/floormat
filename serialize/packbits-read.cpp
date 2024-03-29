#include "packbits-read.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"

namespace floormat {

using namespace floormat::Pack_impl;

namespace {

static_assert(sum<1, 2, 3, 4, 5> == 6*(6-1)/2);
static_assert(sum<5, 10, 15> == 30);

using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

template<std::unsigned_integral T, size_t N> constexpr inline T lowbits = N == sizeof(T)*8 ? (T)-1 : (T{1} << N)-T{1};

static_assert(!input<uint32_t, 3>{65535}.check_zero());
static_assert(input<uint32_t, 30>{65535}.advance<16>() == 0);

static_assert(input<uint32_t, 30>::next<16>{ input<uint32_t, 30>{65535}.advance<16>() }.check_zero());
static_assert(input<uint32_t, 30>::next<16>{}.Left == 14);

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

template<size_t N> using f32 = input_field<uint32_t, N>;
template<size_t N> using f8 = input_field<uint8_t, N>;

constexpr bool test4()
{

    {
        static_assert(lowbits<uint32_t, 17> == 0x1ffffU);
        //auto tuple = std::tuple<f32<17>&, f32<14>&, f32<1>&>{a, b, c};
        f32<17> a; f32<14> b; f32< 1> c;
        auto tuple = std::tie(a, b, c);
        read_(tuple, input<uint32_t, 32>{(uint32_t)-1}, std::make_index_sequence<3>{});
        fm_assert(a == lowbits<uint32_t, 17>);
        fm_assert(b == lowbits<uint32_t, 14>);
        fm_assert(c & 1);
    }
    {
        f8<1> a;
        f8<3> b;
        f8<2> c;
        read_(std::tie(a, b, c), input<uint8_t, 8>{0b101011}, std::make_index_sequence<3>{});
        fm_assert(a == 0b1);
        fm_assert(b == 0b101);
        fm_assert(c == 0b10);
    }
    {
        read_(std::tuple<>{}, input<uint8_t, 8>{0}, std::index_sequence<>{});
        //f32<2> a, b, c;
        //read_(std::tuple<>{}, input<uint8_t, 8>{1}, std::index_sequence<>{});
        //read_(std::tie(a, b, c), input<uint8_t, 5>{0b11111}, std::make_index_sequence<3>{});
        //(void)input<uint8_t, 9>{};
        //read_(std::tie(a), input<uint8_t, 8>{3}, std::index_sequence<0>{}); fm_assert(a == 3);
        //f8<1> d; read_(std::tie(d), input<uint8_t, 8>{1}, std::index_sequence<>{});
    }
    {
        f8<1> a; f8<3> b; f8<2> c;
        pack_read(std::tie(a, b, c), uint8_t{0b101011});
        fm_assert(a == 0b1);
        fm_assert(b == 0b101);
        fm_assert(c == 0b10);
    }

    return true;
}
static_assert(test4());

} // namespace

} // namespace floormat
