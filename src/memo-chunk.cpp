#include "memo-chunk.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "global-coords.hpp"
#include "compat/assert.hpp"
#include <array>
#include <mg/Functions.h>

namespace floormat::detail {

namespace {
inline chunk* uninit_sentinel() noexcept { return reinterpret_cast<chunk*>(~uintptr_t{0}); }
} // namespace

memo_chunk::memo_chunk()
    : _data{NoInit, z_count}
{
    for (auto& p : _data) p = uninit_sentinel();
}

memo_chunk::~memo_chunk() noexcept = default;

size_t memo_chunk::index_of(chunk_coords_ ch) const
{
    int side = 2*_extent + 1;
    int xi = (int)ch.x + _extent;
    int yi = (int)ch.y + _extent;
    int zi = (int)ch.z - chunk_z_min;
    return ((size_t)zi * (size_t)side + (size_t)yi) * (size_t)side + (size_t)xi;
}

void memo_chunk::grow_for(world& w, chunk_coords_ trigger_ch)
{
    int needed = Math::max({_extent,
                            Math::abs((int)trigger_ch.x),
                            Math::abs((int)trigger_ch.y)});
    for (auto& c : w.chunks())
    {
        auto cc = c.coord();
        needed = Math::max({needed, Math::abs((int)cc.x), Math::abs((int)cc.y)});
    }

    int old_extent = _extent;
    int old_side = 2*old_extent + 1;
    Array<chunk*> old_data = move(_data);

    _extent = needed + needed/2 + 4;
    size_t new_side = (size_t)(2*_extent + 1);
    _data = Array<chunk*>{NoInit, new_side*new_side*(size_t)z_count};
    for (auto& p : _data) p = uninit_sentinel();

    for (int zi = 0; zi < z_count; zi++)
        for (int yi = 0; yi < old_side; yi++)
            for (int xi = 0; xi < old_side; xi++)
            {
                size_t old_idx = ((size_t)zi * (size_t)old_side + (size_t)yi) * (size_t)old_side + (size_t)xi;
                chunk* slot = old_data[old_idx];
                if (slot == uninit_sentinel())
                    continue;
                chunk_coords_ ch{
                    (int16_t)(xi - old_extent),
                    (int16_t)(yi - old_extent),
                    (int8_t)(zi + chunk_z_min),
                };
                _data[index_of(ch)] = slot;
            }
}

chunk* memo_chunk::chunk_at(world& w, chunk_coords_ ch)
{
    fm_debug_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);

    if (Math::abs((int)ch.x) > _extent || Math::abs((int)ch.y) > _extent)
        grow_for(w, ch);

    auto& slot = _data[index_of(ch)];
    if (slot == uninit_sentinel())
    {
        slot = w.at(ch);
    }
    return slot;
}

std::array<chunk*, 8> memo_chunk::neighbors(world& w, chunk_coords_ ch0)
{
    std::array<chunk*, 8> ret;
    for (auto i = 0u; i < 8; i++)
        ret[i] = chunk_at(w, ch0 + world::neighbor_offsets[i]);
    return ret;
}

void memo_chunk::update_slot(world& w, chunk_coords_ ch, chunk* p)
{
    fm_debug_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    if (Math::abs((int)ch.x) > _extent || Math::abs((int)ch.y) > _extent)
        grow_for(w, ch);
    _data[index_of(ch)] = p;
}

void memo_chunk::prepare_next_frame(world& w)
{
    (void)w;
#ifndef FM_NO_DEBUG2
    check_in_sync(w);
#endif
}

#ifndef FM_NO_DEBUG2
void memo_chunk::check_in_sync(world& w) const
{
    int side = 2*_extent + 1;
    for (int zi = 0; zi < z_count; zi++)
        for (int yi = 0; yi < side; yi++)
            for (int xi = 0; xi < side; xi++)
            {
                size_t idx = ((size_t)zi * (size_t)side + (size_t)yi) * (size_t)side + (size_t)xi;
                chunk* slot = _data[idx];
                if (slot == uninit_sentinel())
                    continue;
                chunk_coords_ ch{
                    (int16_t)(xi - _extent),
                    (int16_t)(yi - _extent),
                    (int8_t)(zi + chunk_z_min),
                };
                fm_assert(slot == w.at(ch));
            }

    for (auto& c : w.chunks())
    {
        auto ch = c.coord();
        if (Math::abs((int)ch.x) > _extent || Math::abs((int)ch.y) > _extent)
            continue;
        fm_assert(_data[index_of(ch)] == &c);
    }
}
#endif

} // namespace floormat::detail
