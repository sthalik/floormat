#include "chunk-table.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "compat/assert.hpp"

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

struct biased_coords { uint32_t x, y, z; };

inline biased_coords bias(chunk_coords_ ch) noexcept
{
    return { uint16_t(uint32_t(ch.x) + chunk_bias), uint16_t(uint32_t(ch.y) + chunk_bias), uint32_t(ch.z - chunk_z_min) };
}

} // namespace

chunk* chunk_table::chunk_at(chunk_coords_ ch) noexcept
{
    fm_debug3_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    const auto b = bias(ch);
    const auto* s = _table.find(b.x, b.y, b.z);
    return s ? *s : nullptr;
}

const chunk* chunk_table::chunk_at(chunk_coords_ ch) const noexcept
{
    return const_cast<chunk_table*>(this)->chunk_at(ch);
}

void chunk_table::update_slot(chunk_coords_ ch, chunk* p) noexcept
{
    fm_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    const auto b = bias(ch);
    if (p)
    {
        const bool ok = _table.insert(b.x, b.y, b.z, p);
        fm_assert(ok);
    }
    else
        (void)_table.erase(b.x, b.y, b.z);
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
