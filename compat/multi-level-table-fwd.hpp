#pragma once

namespace floormat {

enum class mlt_source : u8 { heap, superpage };

struct mlt_bits
{
    u32 dim[3] = {};
    u32 dims = 0;

    constexpr mlt_bits() noexcept = default;
    constexpr mlt_bits(u32 x) noexcept;
    constexpr mlt_bits(u32 x, u32 y, u32 z) noexcept;

    constexpr u32 total() const noexcept;
};

struct mlt_level
{
    mlt_bits bits;
    bool dynamic = false;
    // index 0 lives in the parent's entry, the others in an array allocated on first use
    bool inline_zero = false;
};

// Complete here because a class-type template parameter can't have an incomplete type.
struct mlt_params
{
    static constexpr u32 max_depth = 4;

    // [0] takes the key's highest bits
    mlt_level levels[max_depth] = {};
    mlt_source top_source = mlt_source::heap;
    mlt_source page_source = mlt_source::heap;
    bool free_empty = false;

    consteval u32 depth() const;
    consteval u32 dims() const;
    consteval u32 dim_bits(u32 d) const;
    consteval u32 key_bits() const;
    consteval bool has_pages() const;
    consteval u32 page_bits() const;
    consteval u32 zero_bits() const;
    consteval u32 top_bits() const;
    consteval mlt_params validate() const;
};

struct mlt_coords { u32 c[3]; };

template<typename C, mlt_params P> struct mlt_coord_traits;

template<typename T, mlt_params P> class multi_level_table;

} // namespace floormat
