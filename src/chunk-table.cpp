#include "chunk-table.inl"
#include "world.hpp"
#include "chunk.hpp"
#include "compat/assert.hpp"

namespace floormat::detail {

void chunk_table::update_slot(chunk_coords_ ch, chunk* p) noexcept
{
    if (p)
    {
        const bool ok = _table.insert(ch, p);
        fm_assert(ok);
        if (_z_counts.isEmpty()) [[unlikely]]
            _z_counts = Array<u32>{ValueInit, usize{chunk_z_count}};
        _z_counts[u32(ch.z - chunk_z_min)]++;
    }
    else if (_table.erase(ch))
    {
        fm_debug_assert(_z_counts[u32(ch.z - chunk_z_min)] > 0);
        _z_counts[u32(ch.z - chunk_z_min)]--;
    }
}

bool chunk_table::has_chunks_at_z(i8 z) const noexcept
{
    return !_z_counts.isEmpty() && _z_counts[u32(z - chunk_z_min)] != 0;
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
