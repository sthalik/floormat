#pragma once

namespace floormat {

enum class mlt_source : uint8_t { heap, superpage };

struct mlt_bits
{
    uint32_t dim[3] = {};
    uint32_t dims = 0;

    constexpr mlt_bits() noexcept = default;
    constexpr mlt_bits(uint32_t x) noexcept;
    constexpr mlt_bits(uint32_t x, uint32_t y, uint32_t z) noexcept;

    constexpr uint32_t total() const noexcept;
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
    static constexpr uint32_t max_depth = 4;

    // [0] takes the key's highest bits
    mlt_level levels[max_depth] = {};
    mlt_source top_source = mlt_source::heap;
    mlt_source page_source = mlt_source::heap;
    bool free_empty = false;

    consteval uint32_t depth() const;
    consteval uint32_t dims() const;
    consteval uint32_t dim_bits(uint32_t d) const;
    consteval uint32_t key_bits() const;
    consteval bool has_pages() const;
    consteval uint32_t page_bits() const;
    consteval uint32_t zero_bits() const;
    consteval uint32_t top_bits() const;
    consteval mlt_params validate() const;
};

struct mlt_coords { uint32_t c[3]; };

template<typename C, mlt_params P> struct mlt_coord_traits;

template<typename T, mlt_params P> class multi_level_table;

} // namespace floormat
