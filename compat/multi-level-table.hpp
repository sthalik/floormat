#pragma once
#include "multi-level-table-fwd.hpp"
#include "superpage.hpp"
#include "compat/assert.hpp"
#include <type_traits>
#include <cr/Array.h>

namespace floormat {

constexpr mlt_bits::mlt_bits(u32 x) noexcept : dim{x, 0, 0}, dims{1} {}
constexpr mlt_bits::mlt_bits(u32 x, u32 y, u32 z) noexcept : dim{x, y, z}, dims{3} {}

constexpr u32 mlt_bits::total() const noexcept { return dim[0] + dim[1] + dim[2]; }

consteval u32 mlt_params::depth() const
{
    u32 n = 0;
    while (n < max_depth && levels[n].bits.total())
        n++;
    return n;
}

consteval u32 mlt_params::dims() const { return levels[0].bits.dims; }

consteval u32 mlt_params::dim_bits(u32 d) const
{
    u32 n = 0;
    for (u32 i = 0; i < depth(); i++)
        n += levels[i].bits.dim[d];
    return n;
}

consteval u32 mlt_params::key_bits() const
{
    u32 n = 0;
    for (u32 i = 0; i < depth(); i++)
        n += levels[i].bits.total();
    return n;
}

consteval bool mlt_params::has_pages() const { return depth() > 0 && levels[depth() - 1].dynamic; }
consteval u32 mlt_params::page_bits() const { return has_pages() ? levels[depth() - 1].bits.total() : 0; }

consteval u32 mlt_params::zero_bits() const
{
    for (u32 i = 0; i < depth(); i++)
        if (levels[i].inline_zero)
            return levels[i].bits.total();
    return 0;
}

consteval u32 mlt_params::top_bits() const { return key_bits() - page_bits() - zero_bits(); }

consteval mlt_params mlt_params::validate() const
{
    const u32 n = depth();
    fm_assert(n > 0);
    for (u32 i = n; i < max_depth; i++)
        fm_assert(!levels[i].bits.total() && !levels[i].dynamic && !levels[i].inline_zero);
    fm_assert(dims() == 1 || dims() == 3);
    for (u32 i = 0; i < n; i++)
    {
        fm_assert(levels[i].bits.dims == dims());
        for (u32 d = 0; d < 3; d++)
            fm_assert(levels[i].bits.dim[d] <= 64);
    }
    fm_assert(key_bits() <= 64);
    if (dims() > 1)
        for (u32 d = 0; d < dims(); d++)
            fm_assert(dim_bits(d) <= 32);
    u32 num_inline_zero = 0;
    for (u32 i = 0; i < n; i++)
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
    static constexpr u32 top_size = 1u << P.top_bits();
    static constexpr u32 page_size = 1u << P.page_bits();
    static constexpr u32 page_mask = page_size - 1;
    static constexpr u32 zero_mask = (1u << P.zero_bits()) - 1;
    static constexpr u32 depth = P.depth();

    template<typename C>
    static constexpr bool is_coord = P.dims() == 3 && requires (const C& c) { mlt_coord_traits<C, P>::coords(c); };

    static constexpr u64 pack_coords(mlt_coords c) noexcept;

    struct counted_page { T* page; u32 live; };
    using page_ref = std::conditional_t<P.free_empty, counted_page, T*>;
    struct split_entry { page_ref zero; page_ref* side; };
    using entry = std::conditional_t<!P.has_pages(), T,
                  std::conditional_t<(P.zero_bits() > 0), split_entry, page_ref>>;

    struct page_record
    {
        T* page = nullptr;
        u32 top_index = 0, zero_index = 0;
        bool large = false;
    };

    page_ref* ref_at(u32 top_index, u32 zero_index) noexcept requires (P.has_pages());
    void detach(const page_record& rec) noexcept;
    T* add_page(u32 top_index, u32 zero_index) noexcept;
    void release_page(u32 top_index, u32 zero_index, T* page) noexcept requires (P.free_empty);
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
    static constexpr u32 key_bits = P.key_bits();
    static constexpr u32 page_bits = P.page_bits();
    static constexpr u32 zero_bits = P.zero_bits();
    static constexpr u32 top_bits = P.top_bits();
    static constexpr bool has_pages = P.has_pages();
    static constexpr u32 dims = P.dims();

    multi_level_table() noexcept;
    ~multi_level_table() noexcept;
    multi_level_table(multi_level_table&& other) noexcept;
    multi_level_table& operator=(multi_level_table&& other) noexcept;

    // the slot may hold T{}
    const T* find(u64 key) const noexcept;
    [[nodiscard]] bool insert(u64 key, T value) noexcept;
    // the caller destroys the result, after the table is consistent
    [[nodiscard]] T erase(u64 key) noexcept;
    // a T destructor may call find(), insert() and erase(), but not clear()
    void clear() noexcept;
    u32 page_count() const noexcept;
    u64 size() const noexcept requires (P.free_empty);

    template<typename C> static constexpr u64 pack(const C& c) noexcept requires is_coord<C> { return pack_coords(mlt_coord_traits<C, P>::coords(c)); }
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
constexpr u64 multi_level_table<T, P>::pack_coords(mlt_coords c) noexcept
{
    u64 key = 0;
    u32 key_shift = 0, coord_shift[3] = {};
    for (u32 i = depth; i-- > 0; )
        for (u32 d = 0; d < dims; d++)
        {
            const u32 bits = P.levels[i].bits.dim[d];
            const u64 mask = (u64{1} << bits) - 1;
            key |= (u64{c.c[d]} >> coord_shift[d] & mask) << key_shift;
            coord_shift[d] += bits;
            key_shift += bits;
        }
    return key;
}

} // namespace floormat
