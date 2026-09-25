#include "chunk-table.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "compat/assert.hpp"

namespace floormat::detail {

namespace {

constexpr uint32_t side_bits  = 9;
constexpr uint32_t outer_bits = chunk_coord_bits - side_bits;
constexpr uint32_t side_mask  = (1u << side_bits) - 1;
constexpr uint32_t z_bits     = 4;
// chunk 0 sits at the centre of its page, so a world around the origin takes one page
constexpr uint32_t chunk_bias = (1u << (chunk_coord_bits - 1)) + (1u << (side_bits - 1));

using table = multi_level_table<chunk*, chunk_table_params>;
static_assert(table::page_bits == 2 * side_bits);
static_assert(table::key_bits == 2 * outer_bits + z_bits + 2 * side_bits);
static_assert(chunk_z_count <= 1 << z_bits);

inline uint64_t make_key(chunk_coords_ ch) noexcept
{
    const uint32_t bx = uint16_t(uint32_t(ch.x) + chunk_bias);
    const uint32_t by = uint16_t(uint32_t(ch.y) + chunk_bias);
    const uint32_t bz = uint32_t(ch.z - chunk_z_min);
    return uint64_t(by >> side_bits) << (outer_bits + z_bits + 2 * side_bits)
         | uint64_t(bx >> side_bits) << (z_bits + 2 * side_bits)
         | uint64_t(bz) << (2 * side_bits)
         | (by & side_mask) << side_bits
         | (bx & side_mask);
}

} // namespace

chunk* chunk_table::chunk_at(chunk_coords_ ch) noexcept
{
    fm_debug3_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    const auto* s = _table.find(make_key(ch));
    return s ? *s : nullptr;
}

const chunk* chunk_table::chunk_at(chunk_coords_ ch) const noexcept
{
    return const_cast<chunk_table*>(this)->chunk_at(ch);
}

void chunk_table::update_slot(chunk_coords_ ch, chunk* p) noexcept
{
    fm_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    const auto key = make_key(ch);
    if (p)
    {
        const bool ok = _table.insert(key, p);
        fm_assert(ok);
    }
    else
        (void)_table.erase(key);
}

std::array<chunk*, 8> chunk_table::neighbors(chunk_coords_ ch0) noexcept
{
    std::array<chunk*, 8> ret;
    for (auto i = 0u; i < 8; i++)
        ret[i] = chunk_at(ch0 + world::neighbor_offsets[i]);
    return ret;
}

std::array<const chunk*, 8> chunk_table::neighbors(chunk_coords_ ch0) const noexcept
{
    std::array<const chunk*, 8> ret;
    for (auto i = 0u; i < 8; i++)
        ret[i] = chunk_at(ch0 + world::neighbor_offsets[i]);
    return ret;
}

#ifndef FM_NO_DEBUG3
void chunk_table::check_in_sync(const world& w) const
{
    for (const auto& c : w.chunks())
        fm_assert(chunk_at(c.coord()) == &c);
}
#endif

} // namespace floormat::detail
