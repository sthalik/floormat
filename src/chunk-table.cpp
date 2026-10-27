#include "chunk-table.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "compat/assert.hpp"
#include "compat/non-const.hpp"
#include <utility>

namespace floormat::detail {

namespace {

constexpr uint32_t side_bits  = 9;
constexpr uint32_t outer_bits = chunk_coord_bits - side_bits;
constexpr uint32_t z_bits     = 4;
// chunk 0 sits at the centre of its page, so a world around the origin takes one page
constexpr uint32_t chunk_bias = (1u << (chunk_coord_bits - 1)) + (1u << (side_bits - 1));

using table = multi_level_table<chunk*, chunk_table_params>;
static_assert(table::dims == 3);
static_assert(table::page_bits == 2 * side_bits);
static_assert(table::key_bits == 2 * outer_bits + z_bits + 2 * side_bits);
static_assert(chunk_z_count <= 1 << z_bits);

} // namespace

} // namespace floormat::detail

namespace floormat {

template<>
struct mlt_coord_traits<chunk_coords_, detail::chunk_table_params>
{
    static constexpr mlt_coords coords(chunk_coords_ ch) noexcept
    {
        return {{ uint16_t(uint32_t(ch.x) + detail::chunk_bias), uint16_t(uint32_t(ch.y) + detail::chunk_bias), uint32_t(ch.z - chunk_z_min) }};
    }
};

} // namespace floormat

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

void chunk_table::update_slot(chunk_coords_ ch, chunk* p) noexcept
{
    if (p)
    {
        const bool ok = _table.insert(ch, p);
        fm_assert(ok);
    }
    else
        (void)_table.erase(ch);
}

std::array<chunk*, 8> chunk_table::neighbors(chunk_coords_ ch0) noexcept
{
    const auto c = std::as_const(*this).neighbors(ch0);
    std::array<chunk*, 8> ret;
    for (auto i = 0u; i < 8; i++)
        ret[i] = non_const(c[i]);
    return ret;
}

std::array<const chunk*, 8> chunk_table::neighbors(chunk_coords_ ch0) const noexcept
{
    constexpr uint32_t side = 1u << side_bits;
    std::array<const chunk*, 8> ret;
    // A page's slot index holds x in its low side_bits and y in the next side_bits, so away from the
    // page's edges all 8 neighbors share ch0's page and one lookup finds them. A missing page holds
    // none of them. lx - 1 < side - 2 holds for lx in [1, side - 2]. For lx == 0 the uint32_t
    // subtraction wraps to a value above side - 2, so the edge takes the slow path. Likewise for ly.
    const auto lx = (uint32_t(ch0.x) + chunk_bias) & (side - 1), ly = (uint32_t(ch0.y) + chunk_bias) & (side - 1);
    if (lx - 1 < side - 2 && ly - 1 < side - 2) [[likely]]
    {
        const auto* s = _table.find(ch0);
        for (auto i = 0u; i < 8; i++)
        {
            const auto off = world::neighbor_offsets[i];
            ret[i] = s ? s[int32_t{off.y()} * int32_t{side} + off.x()] : nullptr;
        }
    }
    else
        for (auto i = 0u; i < 8; i++)
            ret[i] = chunk_at(ch0 + world::neighbor_offsets[i]);
    return ret;
}

const multi_level_table<chunk*, chunk_table_params>& chunk_table::raw_table() const noexcept
{
    return _table;
}

#ifndef FM_NO_DEBUG3
void chunk_table::check_in_sync(const world& w) const
{
    for (const auto& c : w.chunks())
        fm_assert(chunk_at(c.coord()) == &c);
}
#endif

} // namespace floormat::detail
