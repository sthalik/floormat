#include "chunk-table.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "compat/assert.hpp"
#include <cstring>

namespace floormat::detail {

namespace {

constexpr uint32_t total_xbits = 15, total_ybits = 15;
constexpr uint32_t outer_xbits = 10, outer_ybits = 10;
constexpr uint32_t inner_xbits = total_xbits - outer_xbits;
constexpr uint32_t inner_ybits = total_ybits - outer_ybits;
constexpr uint32_t outer_xspan = 1u << outer_xbits;
constexpr uint32_t outer_yspan = 1u << outer_ybits;
constexpr uint32_t inner_xspan = 1u << inner_xbits;
constexpr uint32_t inner_yspan = 1u << inner_ybits;
constexpr uint32_t z_bits      = 4;

constexpr int32_t  chunk_xbias = 1 << (total_xbits - 1);
constexpr int32_t  chunk_ybias = 1 << (total_ybits - 1);

constexpr size_t z_entries = size_t(1) << z_bits;

static_assert(outer_xbits + inner_xbits == total_xbits);
static_assert(outer_ybits + inner_ybits == total_ybits);
static_assert(z_entries >= size_t(chunk_z_max - chunk_z_min + 1));
static_assert(chunk_z_min <= 0 && chunk_z_max >= 0);

inline uint32_t bias_x(int16_t x) noexcept
{
    return uint32_t(int32_t(x) + chunk_xbias);
}

inline uint32_t bias_y(int16_t y) noexcept
{
    return uint32_t(int32_t(y) + chunk_ybias);
}

inline uint32_t bias_z(int8_t z) noexcept
{
    return uint32_t(z - chunk_z_min);
}

} // namespace

struct chunk_table_inner
{
    chunk** data;
    uint8_t xmin, xmax, ymin, ymax;
};
static_assert(sizeof(chunk_table_inner) == 16);

struct chunk_table_outer
{
    chunk_table_inner  inner[1 << outer_ybits][1 << outer_xbits];
    chunk_table_inner* zs[1 << outer_ybits][1 << outer_xbits];
};

chunk_table::chunk_table()
{
    _outer_alloc = superpage_alloc(sizeof(chunk_table_outer));
    _outer = static_cast<chunk_table_outer*>(_outer_alloc.ptr);
}

chunk_table::~chunk_table() noexcept
{
    for (uint32_t y = 0; y < outer_yspan; ++y)
        for (uint32_t x = 0; x < outer_xspan; ++x)
            delete[] _outer->inner[y][x].data;

    for (uint32_t y = 0; y < outer_yspan; ++y)
        for (uint32_t x = 0; x < outer_xspan; ++x)
            if (chunk_table_inner* bucket = _outer->zs[y][x])
            {
                for (uint32_t z = 0; z < z_entries; ++z)
                    delete[] bucket[z].data;
                delete[] bucket;
            }

    superpage_free(_outer_alloc);
}

chunk* chunk_table::chunk_at(chunk_coords_ ch) noexcept
{
    fm_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    fm_assert(uint32_t(ch.x + chunk_xbias) < uint32_t(chunk_xbias) * 2u
           && uint32_t(ch.y + chunk_ybias) < uint32_t(chunk_ybias) * 2u);

    uint32_t bx = bias_x(ch.x), by = bias_y(ch.y);
    uint32_t ox = bx >> inner_xbits, oy = by >> inner_ybits;
    uint32_t lx = bx & (inner_xspan - 1), ly = by & (inner_yspan - 1);

    chunk_table_inner* cell;
    if (ch.z == 0)
        cell = &_outer->inner[oy][ox];
    else
    {
        chunk_table_inner* bucket = _outer->zs[oy][ox];
        if (!bucket)
            return nullptr;
        cell = &bucket[bias_z(ch.z)];
    }

    if (!cell->data)
        return nullptr;
    if (lx < cell->xmin || lx > cell->xmax || ly < cell->ymin || ly > cell->ymax)
        return nullptr;
    uint32_t w = uint32_t(cell->xmax - cell->xmin) + 1;
    return cell->data[(ly - cell->ymin) * w + (lx - cell->xmin)];
}

const chunk* chunk_table::chunk_at(chunk_coords_ ch) const noexcept
{
    return const_cast<chunk_table*>(this)->chunk_at(ch);
}

void chunk_table::update_slot(chunk_coords_ ch, chunk* p) noexcept
{
    fm_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    fm_assert(uint32_t(ch.x + chunk_xbias) < uint32_t(chunk_xbias) * 2u
           && uint32_t(ch.y + chunk_ybias) < uint32_t(chunk_ybias) * 2u);

    uint32_t bx = bias_x(ch.x), by = bias_y(ch.y);
    uint32_t ox = bx >> inner_xbits, oy = by >> inner_ybits;
    uint32_t lx = bx & (inner_xspan - 1), ly = by & (inner_yspan - 1);

    chunk_table_inner* cell;
    if (ch.z == 0)
    {
        cell = &_outer->inner[oy][ox];
    }
    else
    {
        chunk_table_inner*& bucket = _outer->zs[oy][ox];
        if (!bucket)
        {
            if (!p)
                return;
            bucket = new chunk_table_inner[z_entries]{};
        }
        cell = &bucket[bias_z(ch.z)];
    }

    if (p)
    {
        if (!cell->data)
        {
            cell->data = new chunk*[1]{};
            cell->xmin = cell->xmax = uint8_t(lx);
            cell->ymin = cell->ymax = uint8_t(ly);
        }
        else if (lx < cell->xmin || lx > cell->xmax || ly < cell->ymin || ly > cell->ymax)
        {
            uint8_t new_xmin = lx < cell->xmin ? uint8_t(lx) : cell->xmin;
            uint8_t new_xmax = lx > cell->xmax ? uint8_t(lx) : cell->xmax;
            uint8_t new_ymin = ly < cell->ymin ? uint8_t(ly) : cell->ymin;
            uint8_t new_ymax = ly > cell->ymax ? uint8_t(ly) : cell->ymax;
            uint32_t old_w = uint32_t(cell->xmax - cell->xmin) + 1;
            uint32_t old_h = uint32_t(cell->ymax - cell->ymin) + 1;
            uint32_t new_w = uint32_t(new_xmax - new_xmin) + 1;
            uint32_t new_h = uint32_t(new_ymax - new_ymin) + 1;
            chunk** new_data = new chunk*[size_t(new_w) * new_h]{};
            uint32_t x_off = uint32_t(cell->xmin - new_xmin);
            uint32_t y_off = uint32_t(cell->ymin - new_ymin);
            for (uint32_t y = 0; y < old_h; ++y)
            {
                chunk** src = cell->data + y * old_w;
                chunk** dst = new_data + (y + y_off) * new_w + x_off;
                std::memcpy(dst, src, sizeof(chunk*) * old_w);
            }
            delete[] cell->data;
            cell->data = new_data;
            cell->xmin = new_xmin; cell->xmax = new_xmax;
            cell->ymin = new_ymin; cell->ymax = new_ymax;
        }
        uint32_t w = uint32_t(cell->xmax - cell->xmin) + 1;
        cell->data[(ly - cell->ymin) * w + (lx - cell->xmin)] = p;
    }
    else
    {
        if (!cell->data)
            return;
        if (lx < cell->xmin || lx > cell->xmax || ly < cell->ymin || ly > cell->ymax)
            return;
        uint32_t w = uint32_t(cell->xmax - cell->xmin) + 1;
        cell->data[(ly - cell->ymin) * w + (lx - cell->xmin)] = nullptr;
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
