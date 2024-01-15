#include "packbits-write.hpp"

namespace floormat::detail_Pack_output {

using u32 = uint32_t;
using u8 = uint8_t;

template u32 write_(output<u32, 32>, output_field<u32, 1>);

static_assert(output<u32, 32>::next<1>::Capacity == 31);
static_assert(output<u32, 32>::next<1>::next<2>::Capacity == 29);

} // namespace floormat::detail_Pack_output
