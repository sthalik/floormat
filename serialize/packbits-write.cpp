#include "packbits-write.hpp"

namespace floormat::detail_Pack_output {

using u32 = uint32_t;
using u8 = uint8_t;
template<size_t N> using f32 = output_field<u32, N>;
template<size_t N> using f8 = output_field<u8, N>;

static_assert(count_bits<u32, std::tuple< f32<2>, f32<3>, f32<5> >> == 10);
static_assert(count_bits<uint8_t, std::tuple< f8<1>, f8<2>, f8<4> >> == 7);
static_assert(count_bits<u8, std::tuple<>> == 0);
//static_assert(count_bits<u8, std::tuple< f8<9> >> == 0);
//static_assert(count_bits<u8, std::tuple< f8<7>, f8<2> >> == 9);

template u32 write_(const std::tuple<f32<1>>&, output<u32, 32>, output_bits<32>, std::index_sequence<0>);
static_assert(write_(std::tuple<output_field<u32, 13>>{4242}, output<u32, 32>{0}, output_bits<32>{}, std::index_sequence<0>{}) == 4242);

static_assert(write_(
    std::tuple{f8<3>{7}, f8<2>{3}, f8<1>{1}},
    output<u8, 6>{0},
    output_bits<8>{},
    std::make_index_sequence<3>{}
) == (1 << 6) - 1);

static_assert(write_(
    std::tuple{f32<2>{0b10}, f32<3>{0b011}, f32<3>{0b001}},
    output<u32, 32>{0},
    output_bits<32>{},
    make_reverse_index_sequence<3>{}) == 0b000101110);

} // namespace floormat::detail_Pack_output
