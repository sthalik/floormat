#pragma once
#include "compat/multi-level-table.hpp"
#include <array>

namespace floormat {
class world;
class chunk;
struct chunk_coords_;
} // namespace floormat

namespace floormat::detail {

// key: oy:7 | ox:7 | z:4 | ly:9 | lx:9
constexpr inline mlt_params chunk_table_params = mlt_params{
    .levels = { {.bits = {7, 7, 0}}, {.bits = {0, 0, 4}}, {.bits = {9, 9, 0}, .dynamic = true}, },
    .top_source = mlt_source::superpage,
    .page_source = mlt_source::superpage,
}.validate();

class chunk_table
{
public:
    chunk*       chunk_at(chunk_coords_ ch) noexcept;
    const chunk* chunk_at(chunk_coords_ ch) const noexcept;

    std::array<chunk*, 8>       neighbors(chunk_coords_ ch0) noexcept;
    std::array<const chunk*, 8> neighbors(chunk_coords_ ch0) const noexcept;

    void update_slot(chunk_coords_ ch, chunk* p) noexcept;

#ifndef FM_NO_DEBUG3
    void check_in_sync(const world& w) const;
#endif

    // tests
    const multi_level_table<chunk*, chunk_table_params>& raw_table() const noexcept;

private:
    multi_level_table<chunk*, chunk_table_params> _table;
};

} // namespace floormat::detail
