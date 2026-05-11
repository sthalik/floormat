#include "chunk-table.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "compat/assert.hpp"

namespace floormat::detail {

namespace {

constexpr uint32_t total_xbits = 14, total_ybits = 14;
constexpr uint32_t outer_xbits = 11, outer_ybits = 11;
constexpr uint32_t leaf_xbits  = total_xbits - outer_xbits;
constexpr uint32_t leaf_ybits  = total_ybits - outer_ybits;
constexpr uint32_t z_bits      = 4;

constexpr size_t outer_entries = size_t(1) << (outer_xbits + outer_ybits);
constexpr size_t leaf_entries  = size_t(1) << (leaf_xbits  + leaf_ybits);
constexpr size_t z_entries     = size_t(1) << z_bits;

constexpr uint32_t leaf_xmask = (1u << leaf_xbits) - 1;
constexpr uint32_t leaf_ymask = (1u << leaf_ybits) - 1;

static_assert(outer_xbits + leaf_xbits == total_xbits);
static_assert(outer_ybits + leaf_ybits == total_ybits);
static_assert(z_entries >= size_t(chunk_z_max - chunk_z_min + 1));
static_assert(chunk_z_min <= 0 && chunk_z_max >= 0);

struct z_other
{
    chunk* slots[z_entries] = {};
};

inline uint32_t bias_x(int16_t x) noexcept
{
    return uint32_t(x + (1 << (total_xbits - 1))) & ((1u << total_xbits) - 1);
}

inline uint32_t bias_y(int16_t y) noexcept
{
    return uint32_t(y + (1 << (total_ybits - 1))) & ((1u << total_ybits) - 1);
}

inline uint32_t bias_z(int8_t z) noexcept
{
    return uint32_t(z - chunk_z_min);
}

inline uint32_t outer_idx(uint32_t ux, uint32_t uy) noexcept
{
    return ((ux >> leaf_xbits) << outer_ybits) | (uy >> leaf_ybits);
}

inline uint32_t leaf_idx(uint32_t ux, uint32_t uy) noexcept
{
    return ((ux & leaf_xmask) << leaf_ybits) | (uy & leaf_ymask);
}

} // namespace

struct chunk_table_leaf
{
    chunk* z0[leaf_entries] = {};
    Pointer<z_other> rest[leaf_entries];
};

chunk_table::chunk_table() : _outer{ValueInit, outer_entries} {}
chunk_table::~chunk_table() noexcept = default;

chunk* chunk_table::chunk_at(chunk_coords_ ch) noexcept
{
    fm_debug_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    uint32_t ux = bias_x(ch.x), uy = bias_y(ch.y);
    auto* lp = _outer[outer_idx(ux, uy)].get();
    if (!lp)
        return nullptr;
    uint32_t li = leaf_idx(ux, uy);
    if (ch.z == 0) [[likely]]
        return lp->z0[li];
    auto* zp = lp->rest[li].get();
    if (!zp)
        return nullptr;
    return zp->slots[bias_z(ch.z)];
}

const chunk* chunk_table::chunk_at(chunk_coords_ ch) const noexcept
{
    return const_cast<chunk_table*>(this)->chunk_at(ch);
}

void chunk_table::update_slot(chunk_coords_ ch, chunk* p) noexcept
{
    fm_debug_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    uint32_t ux = bias_x(ch.x), uy = bias_y(ch.y);
    auto& lp = _outer[outer_idx(ux, uy)];
    uint32_t li = leaf_idx(ux, uy);
    if (p)
    {
        if (!lp)
            lp.reset(new chunk_table_leaf{});
        if (ch.z == 0) [[likely]]
        {
            lp->z0[li] = p;
            return;
        }
        auto& zp = lp->rest[li];
        if (!zp)
            zp.reset(new z_other{});
        zp->slots[bias_z(ch.z)] = p;
    }
    else
    {
        if (!lp)
            return;
        if (ch.z == 0) [[likely]]
        {
            lp->z0[li] = nullptr;
            return;
        }
        auto& zp = lp->rest[li];
        if (!zp)
            return;
        zp->slots[bias_z(ch.z)] = nullptr;
    }
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

#ifndef FM_NO_DEBUG2
void chunk_table::check_in_sync(const world& w) const
{
    for (const auto& c : w.chunks())
        fm_assert(chunk_at(c.coord()) == &c);
}
#endif

} // namespace floormat::detail
