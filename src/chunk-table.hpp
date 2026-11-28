#pragma once
#include "compat/multi-level-table.hpp"
#include "tile-defs.hpp"
#include <array>
#include <bit>

namespace floormat {
class world;
class chunk;
struct chunk_coords_;
} // namespace floormat

namespace floormat::detail {

// key: oy | ox | z | ly:9 | lx:9
constexpr inline mlt_params chunk_table_params = mlt_params{
    .levels = {
        {.bits = {chunk_coord_bits - 9, chunk_coord_bits - 9, 0}},
        {.bits = {0, 0, (u32)std::bit_width(u32(chunk_z_count - 1))}},
        {.bits = {9, 9, 0}, .dynamic = true},
    },
    .top_source = mlt_source::superpage,
    .page_source = mlt_source::superpage,
}.validate();

class chunk_table
{
    multi_level_table<chunk*, chunk_table_params> _table;
    // Chunks per z level from chunk_z_min. Empty before the first chunk and after a move.
    Array<u32> _z_counts;

public:
    chunk*       chunk_at(chunk_coords_ ch) noexcept;
    const chunk* chunk_at(chunk_coords_ ch) const noexcept;

    std::array<chunk*, 8>       neighbors(chunk_coords_ ch0) noexcept;
    std::array<const chunk*, 8> neighbors(chunk_coords_ ch0) const noexcept;

    void update_slot(chunk_coords_ ch, chunk* p) noexcept;
    bool has_chunks_at_z(i8 z) const noexcept;

#ifndef FM_NO_DEBUG3
    void check_in_sync(const world& w) const;
#endif

    // tests
    const multi_level_table<chunk*, chunk_table_params>& raw_table() const noexcept;
};

} // namespace floormat::detail
