#pragma once
#include "chunk-table.hpp"
#include "global-coords.hpp"

namespace floormat::detail {

namespace {

constexpr u32 side_bits  = 9;
constexpr u32 outer_bits = chunk_coord_bits - side_bits;
constexpr u32 z_bits     = std::bit_width(u32(chunk_z_count - 1));
// chunk 0 sits at the centre of its page, so a world around the origin takes one page
constexpr u32 chunk_bias = (1u << (chunk_coord_bits - 1)) + (1u << (side_bits - 1));

using table = multi_level_table<chunk*, chunk_table_params>;
static_assert(table::dims == 3);
static_assert(table::page_bits == 2 * side_bits);
static_assert(table::key_bits == 2 * outer_bits + z_bits + 2 * side_bits);

} // namespace

} // namespace floormat::detail

namespace floormat {

template<>
struct mlt_coord_traits<chunk_coords_, detail::chunk_table_params>
{
    static constexpr mlt_coords coords(chunk_coords_ ch) noexcept
    {
        constexpr u32 xy_mask = (1u << chunk_coord_bits) - 1;
        return {{ (u32(ch.x) + detail::chunk_bias) & xy_mask, (u32(ch.y) + detail::chunk_bias) & xy_mask, u32(ch.z - chunk_z_min) }};
    }
};

} // namespace floormat
