#include "packbits-write.hpp"

namespace floormat {

namespace {

using namespace floormat::Pack;

using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;
template<size_t N> using f32 = output_field<u32, N>;
template<size_t N> using f16 = output_field<u16, N>;
template<size_t N> using f8 = output_field<u8, N>;

static_assert(write_(std::tuple<output_field<u32, 13>>{4242}, output<u32, 32, 32>{0}, std::index_sequence<0>{}) == 4242u);

static_assert(write_(
    std::tuple{f8<3>{7}, f8<2>{3}, f8<1>{1}},
    output<u8, 8, 6>{0},
    std::make_index_sequence<3>{}
) == (1 << 6) - 1);

static_assert(write_(
    std::tuple{f32<2>{0b10}, f32<3>{0b011}, f32<3>{0b001}},
    output<u32, 32, 32>{0},
    make_reverse_index_sequence<3>{}) == 0b000101110);

static_assert(pack_write(std::tuple{f32<2>{0b10}, f32<3>{0b011}, f32<3>{0b01}}) == 0b00101110);
//static_assert(pack_write(std::tuple{f32<2>{0b10}, f32<3>{0b1011}, f32<3>{0b001}}) == 0b000101110);
static_assert(pack_write(std::tuple{f8<2>{0b10}, f8<3>{0b011}, f8<3>{0b01}}) == 0b00101110);
//static_assert(pack_write(std::tuple{f8<2>{0b10}, f8<3>{0b011}, f8<4>{0b01}}) == 0b00101110);
//static_assert(pack_write(std::tuple{}) == 0);
static_assert(pack_write(std::tuple{f8<1>{0b1}, f8<3>{0b101}, f8<2>{0b10}}) == 0b101011);

#if 0 // check disasembly
u32 foo1(u32 a, u32 b, u32 c);
u32 foo1(u32 a, u32 b, u32 c)
{
    return pack_write(std::tuple{f32<2>{a}, f32<3>{b}, f32<3>{c}});
}
#endif

} // namespace

} // namespace floormat
