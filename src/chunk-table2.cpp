#include "chunk-table.inl"
#include "world.hpp"
#include "compat/map.hpp"
#include "compat/non-const.hpp"
#include <utility>

namespace floormat::detail {

// neighbors() steps a slot index by x + y * side, so x takes the key's low side_bits and y the next.
static_assert(table::pack(chunk_coords_{1, 0, 0}) - table::pack(chunk_coords_{}) == 1 &&
              table::pack(chunk_coords_{0, 1, 0}) - table::pack(chunk_coords_{}) == 1u << side_bits);

chunk* chunk_table::chunk_at(chunk_coords_ ch) noexcept
{
    return non_const(std::as_const(*this).chunk_at(ch));
}

const chunk* chunk_table::chunk_at(chunk_coords_ ch) const noexcept
{
    const auto* s = _table.find(ch);
    return s ? *s : nullptr;
}

std::array<chunk*, 8> chunk_table::neighbors(chunk_coords_ ch0) noexcept
{
    const auto c = std::as_const(*this).neighbors(ch0);
    return ::floormat::map([](const chunk* x) { return non_const(x); }, c);
}

std::array<const chunk*, 8> chunk_table::neighbors(chunk_coords_ ch0) const noexcept
{
    constexpr u32 side = 1u << side_bits;
    // Away from a page's edges all 8 neighbors share ch0's page, so one lookup finds them, and a
    // missing page holds none. lx - 1 < side - 2 holds for lx in [1, side - 2], and for lx == 0 the
    // u32 subtraction wraps, so the edge takes the slow path. Likewise for ly.
    const auto lx = (u32(ch0.x) + chunk_bias) & (side - 1), ly = (u32(ch0.y) + chunk_bias) & (side - 1);
    if (lx - 1 < side - 2 && ly - 1 < side - 2) [[likely]]
    {
        const auto* s = _table.find(ch0);
        return ::floormat::map([s](Vector2b off) -> const chunk* {
            return s ? s[i32{off.y()} * i32{side} + off.x()] : nullptr;
        }, world::neighbor_offsets);
    }
    else
        return ::floormat::map([&] [[fm_always_inline]] (Vector2b off) { return chunk_at(ch0 + off); }, world::neighbor_offsets);
}

} // namespace floormat::detail
