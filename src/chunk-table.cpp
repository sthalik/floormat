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
    }
    else
        (void)_table.erase(ch);
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
