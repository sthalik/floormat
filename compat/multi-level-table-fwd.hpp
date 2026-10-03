#pragma once
#include "compat/assert.hpp"

namespace floormat {

enum class mlt_source : uint8_t { heap, superpage };

struct mlt_bits
{
    uint32_t dim[3] = {};
    uint32_t dims = 0;

    constexpr mlt_bits() noexcept = default;
    constexpr mlt_bits(uint32_t x) noexcept : dim{x, 0, 0}, dims{1} {}
    constexpr mlt_bits(uint32_t x, uint32_t y, uint32_t z) noexcept : dim{x, y, z}, dims{3} {}

    constexpr uint32_t total() const noexcept { return dim[0] + dim[1] + dim[2]; }
};

struct mlt_level
{
    mlt_bits bits;
    bool dynamic = false;
    // index 0 lives in the parent's entry, the others in an array allocated on first use
    bool inline_zero = false;
};

struct mlt_params
{
    static constexpr uint32_t max_depth = 4;

    // [0] takes the key's highest bits
    mlt_level levels[max_depth] = {};
    mlt_source top_source = mlt_source::heap;
    mlt_source page_source = mlt_source::heap;
    bool free_empty = false;

    consteval uint32_t depth() const
    {
        uint32_t n = 0;
        while (n < max_depth && levels[n].bits.total())
            n++;
        return n;
    }

    consteval uint32_t dims() const { return levels[0].bits.dims; }

    consteval uint32_t dim_bits(uint32_t d) const
    {
        uint32_t n = 0;
        for (uint32_t i = 0; i < depth(); i++)
            n += levels[i].bits.dim[d];
        return n;
    }

    consteval uint32_t key_bits() const
    {
        uint32_t n = 0;
        for (uint32_t i = 0; i < depth(); i++)
            n += levels[i].bits.total();
        return n;
    }

    consteval bool has_pages() const { return depth() > 0 && levels[depth() - 1].dynamic; }
    consteval uint32_t page_bits() const { return has_pages() ? levels[depth() - 1].bits.total() : 0; }

    consteval uint32_t zero_bits() const
    {
        for (uint32_t i = 0; i < depth(); i++)
            if (levels[i].inline_zero)
                return levels[i].bits.total();
        return 0;
    }

    consteval uint32_t top_bits() const { return key_bits() - page_bits() - zero_bits(); }

    consteval mlt_params validate() const
    {
        const uint32_t n = depth();
        fm_assert(n > 0);
        for (uint32_t i = n; i < max_depth; i++)
            fm_assert(!levels[i].bits.total() && !levels[i].dynamic && !levels[i].inline_zero);
        fm_assert(dims() == 1 || dims() == 3);
        for (uint32_t i = 0; i < n; i++)
        {
            fm_assert(levels[i].bits.dims == dims());
            for (uint32_t d = 0; d < 3; d++)
                fm_assert(levels[i].bits.dim[d] <= 64);
        }
        fm_assert(key_bits() <= 64);
        if (dims() > 1)
            for (uint32_t d = 0; d < dims(); d++)
                fm_assert(dim_bits(d) <= 32);
        uint32_t num_inline_zero = 0;
        for (uint32_t i = 0; i < n; i++)
        {
            fm_assert(!levels[i].dynamic || i + 1 == n);
            if (levels[i].inline_zero)
            {
                fm_assert(i + 2 == n && has_pages());
                num_inline_zero++;
            }
        }
        fm_assert(num_inline_zero <= 1);
        fm_assert(!free_empty || has_pages());
        fm_assert(has_pages() || page_source == mlt_source::heap);
        fm_assert(top_bits() < 32 && page_bits() < 32 && zero_bits() < 32);
        return *this;
    }
};

template<typename T, mlt_params P> class multi_level_table;

} // namespace floormat
