#pragma once
#include "multi-level-table-fwd.hpp"
#include "superpage.hpp"
#include "compat/assert.hpp"
#include <type_traits>
#include <cr/Array.h>

namespace floormat {

constexpr mlt_bits::mlt_bits(uint32_t x) noexcept : dim{x, 0, 0}, dims{1} {}
constexpr mlt_bits::mlt_bits(uint32_t x, uint32_t y, uint32_t z) noexcept : dim{x, y, z}, dims{3} {}

constexpr uint32_t mlt_bits::total() const noexcept { return dim[0] + dim[1] + dim[2]; }

consteval uint32_t mlt_params::depth() const
{
    uint32_t n = 0;
    while (n < max_depth && levels[n].bits.total())
        n++;
    return n;
}

consteval uint32_t mlt_params::dims() const { return levels[0].bits.dims; }

consteval uint32_t mlt_params::dim_bits(uint32_t d) const
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < depth(); i++)
        n += levels[i].bits.dim[d];
    return n;
}

consteval uint32_t mlt_params::key_bits() const
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < depth(); i++)
        n += levels[i].bits.total();
    return n;
}

consteval bool mlt_params::has_pages() const { return depth() > 0 && levels[depth() - 1].dynamic; }
consteval uint32_t mlt_params::page_bits() const { return has_pages() ? levels[depth() - 1].bits.total() : 0; }

consteval uint32_t mlt_params::zero_bits() const
{
    for (uint32_t i = 0; i < depth(); i++)
        if (levels[i].inline_zero)
            return levels[i].bits.total();
    return 0;
}

consteval uint32_t mlt_params::top_bits() const { return key_bits() - page_bits() - zero_bits(); }

consteval mlt_params mlt_params::validate() const
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

// A slot is occupied when x.has_block(), or bool(x) for a T without it. An unoccupied T
// must own nothing: pages are freed without destroying such slots.
template<typename T, mlt_params Pʹ>
class multi_level_table final
{
    static constexpr auto P = Pʹ.validate();
    static constexpr uint32_t top_size = 1u << P.top_bits();
    static constexpr uint32_t page_size = 1u << P.page_bits();
    static constexpr uint32_t page_mask = page_size - 1;
    static constexpr uint32_t zero_mask = (1u << P.zero_bits()) - 1;
    static constexpr uint32_t depth = P.depth();

    template<typename C>
    static constexpr bool is_coord = P.dims() == 3 && requires (const C& c) { mlt_coord_traits<C, P>::coords(c); };

    static constexpr uint64_t pack_coords(mlt_coords c) noexcept;

    struct counted_page { T* page; uint32_t live; };
    using page_ref = std::conditional_t<P.free_empty, counted_page, T*>;
    struct split_entry { page_ref zero; page_ref* side; };
    using entry = std::conditional_t<!P.has_pages(), T,
                  std::conditional_t<(P.zero_bits() > 0), split_entry, page_ref>>;

    struct page_record
    {
        T* page = nullptr;
        uint32_t top_index = 0, zero_index = 0;
        bool large = false;
    };

    page_ref* ref_at(uint32_t top_index, uint32_t zero_index) noexcept requires (P.has_pages());
    void detach(const page_record& rec) noexcept;
    T* add_page(uint32_t top_index, uint32_t zero_index) noexcept;
    void release_page(uint32_t top_index, uint32_t zero_index, T* page) noexcept requires (P.free_empty);
    void remove_page(T* page) noexcept;
    void recycle(const page_record& rec) noexcept;
    void free_page(const page_record& rec) noexcept;
    void free_all_pages() noexcept;
    void destroy() noexcept;

    entry* _top = nullptr;
    superpage_alloc_t _top_alloc;
    Array<page_record> _pages;
    page_record _spare;
    bool _clearing = false;

public:
    static constexpr uint32_t key_bits = P.key_bits();
    static constexpr uint32_t page_bits = P.page_bits();
    static constexpr uint32_t zero_bits = P.zero_bits();
    static constexpr uint32_t top_bits = P.top_bits();
    static constexpr bool has_pages = P.has_pages();
    static constexpr uint32_t dims = P.dims();

    multi_level_table() noexcept;
    ~multi_level_table() noexcept;
    multi_level_table(multi_level_table&& other) noexcept;
    multi_level_table& operator=(multi_level_table&& other) noexcept;

    // the slot may hold T{}
    const T* find(uint64_t key) const noexcept;
    [[nodiscard]] bool insert(uint64_t key, T value) noexcept;
    // the caller destroys the result, after the table is consistent
    [[nodiscard]] T erase(uint64_t key) noexcept;
    // a T destructor may call find(), insert() and erase(), but not clear()
    void clear() noexcept;
    uint32_t page_count() const noexcept;
    uint64_t size() const noexcept requires (P.free_empty);

    template<typename C> static constexpr uint64_t pack(const C& c) noexcept requires is_coord<C> { return pack_coords(mlt_coord_traits<C, P>::coords(c)); }
    template<typename C> const T* find(const C& c) const noexcept requires is_coord<C> { return find(pack(c)); }
    template<typename C> [[nodiscard]] bool insert(const C& c, T value) noexcept requires is_coord<C> { return insert(pack(c), move(value)); }
    template<typename C> [[nodiscard]] T erase(const C& c) noexcept requires is_coord<C> { return erase(pack(c)); }

    // tests
    ArrayView<const entry> raw_top() const noexcept;
    ArrayView<const page_record> raw_pages() const noexcept;
    const page_record& raw_spare() const noexcept;
    const superpage_alloc_t& raw_top_alloc() const noexcept;
};

// Levels go outermost first. Inside a level, x takes the lowest bits.
template<typename T, mlt_params P>
constexpr uint64_t multi_level_table<T, P>::pack_coords(mlt_coords c) noexcept
{
    uint64_t key = 0;
    uint32_t key_shift = 0, coord_shift[3] = {};
    for (uint32_t i = depth; i-- > 0; )
        for (uint32_t d = 0; d < dims; d++)
        {
            const uint32_t bits = P.levels[i].bits.dim[d];
            const uint64_t mask = (uint64_t{1} << bits) - 1;
            key |= (uint64_t{c.c[d]} >> coord_shift[d] & mask) << key_shift;
            coord_shift[d] += bits;
            key_shift += bits;
        }
    return key;
}

} // namespace floormat
