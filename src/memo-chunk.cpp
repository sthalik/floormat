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

unsigned max_power_of_2 = 0;
} // namespace

memo_chunk::memo_chunk()
    : _data{NoInit, z_count}
{
    for (auto& p : _data) p = uninit_sentinel();
}

memo_chunk::~memo_chunk() noexcept = default;

size_t memo_chunk::index_of(chunk_coords_ ch) const
{
    int x_side = _x_max - _x_min + 1;
    int y_side = _y_max - _y_min + 1;
    int xi = (int)ch.x - _x_min;
    int yi = (int)ch.y - _y_min;
    int zi = (int)ch.z - chunk_z_min;
    return ((size_t)zi * (size_t)y_side + (size_t)yi) * (size_t)x_side + (size_t)xi;
}

void memo_chunk::grow_for(world& w, chunk_coords_ trigger_ch)
{
    int x_min_req = Math::min(_x_min, (int)trigger_ch.x);
    int x_max_req = Math::max(_x_max, (int)trigger_ch.x);
    int y_min_req = Math::min(_y_min, (int)trigger_ch.y);
    int y_max_req = Math::max(_y_max, (int)trigger_ch.y);
    for (auto& c : w.chunks())
    {
        auto cc = c.coord();
        x_min_req = Math::min(x_min_req, (int)cc.x);
        x_max_req = Math::max(x_max_req, (int)cc.x);
        y_min_req = Math::min(y_min_req, (int)cc.y);
        y_max_req = Math::max(y_max_req, (int)cc.y);
    }

    int old_x_min = _x_min, old_x_max = _x_max;
    int old_y_min = _y_min, old_y_max = _y_max;
    int old_x_side = old_x_max - old_x_min + 1;
    int old_y_side = old_y_max - old_y_min + 1;
    Array<chunk*> old_data = move(_data);

    int x_pad_pos = (x_max_req > 0) ? x_max_req/2 + 4 : 0;
    int x_pad_neg = (x_min_req < 0) ? (-x_min_req)/2 + 4 : 0;
    int y_pad_pos = (y_max_req > 0) ? y_max_req/2 + 4 : 0;
    int y_pad_neg = (y_min_req < 0) ? (-y_min_req)/2 + 4 : 0;

    _x_min = x_min_req - x_pad_neg;
    _x_max = x_max_req + x_pad_pos;
    _y_min = y_min_req - y_pad_neg;
    _y_max = y_max_req + y_pad_pos;

    size_t new_x_side = (unsigned)(_x_max - _x_min + 1);
    size_t new_y_side = (unsigned)(_y_max - _y_min + 1);
    _data = Array<chunk*>{NoInit, new_x_side*new_y_side*(size_t)z_count};
    for (auto& p : _data) p = uninit_sentinel();

    for (int zi = 0; zi < z_count; zi++)
        for (int yi = 0; yi < old_y_side; yi++)
            for (int xi = 0; xi < old_x_side; xi++)
            {
                size_t old_idx = ((size_t)zi * (size_t)old_y_side + (size_t)yi) * (size_t)old_x_side + (size_t)xi;
                chunk_coords_ ch{
                    (int16_t)(xi + old_x_min),
                    (int16_t)(yi + old_y_min),
                    (int8_t)(zi + chunk_z_min),
                };
                _data[index_of(ch)] = old_data[old_idx];
            }

    int big_extent = Math::max({Math::abs(_x_min), _x_max, Math::abs(_y_min), _y_max});
    if (big_extent >= 2048)
    {
        auto POT = Math::log2(Math::max(1u, (unsigned)big_extent));
        if (POT > max_power_of_2)
        {
            max_power_of_2 = POT;
            ERR_nospace << "warning: memo-chunk size is now " << big_extent << " >= 2^" << POT;
        }
    }
}

chunk* memo_chunk::chunk_at(world& w, chunk_coords_ ch)
{
    fm_debug_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);

    if (ch.x < _x_min || ch.x > _x_max || ch.y < _y_min || ch.y > _y_max)
        grow_for(w, ch);

    auto& slot = _data[index_of(ch)];
    if (slot == uninit_sentinel())
        slot = w.at(ch);
    return slot;
}

const chunk* memo_chunk::chunk_at(const world& w, chunk_coords_ ch) const
{
    fm_debug_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    if (ch.x < _x_min || ch.x > _x_max || ch.y < _y_min || ch.y > _y_max)
        return w.at(ch);
    const auto& slot = _data[index_of(ch)];
    if (slot == uninit_sentinel())
        return w.at(ch);
    return slot;
}

std::array<chunk*, 8> memo_chunk::neighbors(world& w, chunk_coords_ ch0)
{
    std::array<chunk*, 8> ret;
    for (auto i = 0u; i < 8; i++)
        ret[i] = chunk_at(w, ch0 + world::neighbor_offsets[i]);
    return ret;
}

std::array<const chunk*, 8> memo_chunk::neighbors(const world& w, chunk_coords_ ch0) const
{
    std::array<const chunk*, 8> ret;
    for (auto i = 0u; i < 8; i++)
        ret[i] = chunk_at(w, ch0 + world::neighbor_offsets[i]);
    return ret;
}

void memo_chunk::update_slot(world& w, chunk_coords_ ch, chunk* p)
{
    fm_debug_assert(ch.z >= chunk_z_min && ch.z <= chunk_z_max);
    if (ch.x < _x_min || ch.x > _x_max || ch.y < _y_min || ch.y > _y_max)
        grow_for(w, ch);
    _data[index_of(ch)] = p;
}

void memo_chunk::prepare_next_frame(world& w)
{
    (void)w;
#ifndef FM_NO_DEBUG2
    check_in_sync(w); // todo debug3
#endif
}

#ifndef FM_NO_DEBUG2
void memo_chunk::check_in_sync(const world& w) const
{
    int x_side = _x_max - _x_min + 1;
    int y_side = _y_max - _y_min + 1;
    for (int zi = 0; zi < z_count; zi++)
        for (int yi = 0; yi < y_side; yi++)
            for (int xi = 0; xi < x_side; xi++)
            {
                size_t idx = ((size_t)zi * (size_t)y_side + (size_t)yi) * (size_t)x_side + (size_t)xi;
                const chunk* slot = _data[idx];
                if (slot == uninit_sentinel())
                    continue;
                chunk_coords_ ch{
                    (int16_t)(xi + _x_min),
                    (int16_t)(yi + _y_min),
                    (int8_t)(zi + chunk_z_min),
                };
                fm_assert(slot == w.at(ch));
            }

    for (auto& c : w.chunks())
    {
        auto ch = c.coord();
        if (ch.x < _x_min || ch.x > _x_max || ch.y < _y_min || ch.y > _y_max)
            continue;
        fm_assert(_data[index_of(ch)] == &c);
    }
}
#endif

} // namespace floormat::detail
