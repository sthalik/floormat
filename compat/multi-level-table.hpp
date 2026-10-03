#pragma once
#include "multi-level-table-fwd.hpp"
#include "superpage.hpp"
#include <type_traits>
#include <cr/Array.h>

namespace floormat {

// A slot is occupied when x.has_block(), or bool(x) for a T without it. An unoccupied T
// must own nothing: pages are freed without destroying such slots.
template<typename T, mlt_params Pʹ>
class multi_level_table final
{
public:
    static constexpr auto P = Pʹ.validate();
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

    static constexpr uint64_t pack(uint32_t x, uint32_t y, uint32_t z) noexcept requires (dims == 3) { return pack_coords({x, y, z}); }

    const T* find(uint32_t x, uint32_t y, uint32_t z) const noexcept requires (dims == 3);
    [[nodiscard]] bool insert(uint32_t x, uint32_t y, uint32_t z, T value) noexcept requires (dims == 3);
    [[nodiscard]] T erase(uint32_t x, uint32_t y, uint32_t z) noexcept requires (dims == 3);

private:
    static constexpr uint32_t top_size = 1u << top_bits;
    static constexpr uint32_t page_size = 1u << page_bits;
    static constexpr uint32_t page_mask = page_size - 1;
    static constexpr uint32_t zero_mask = (1u << zero_bits) - 1;
    static constexpr uint32_t depth = P.depth();
    static constexpr uint32_t dim_bits[3] = { P.dim_bits(0), P.dim_bits(1), P.dim_bits(2) };

    struct coords { uint32_t c[3]; };
    static constexpr uint64_t pack_coords(coords c) noexcept;
    [[noreturn]] static void bad_coords(coords c) noexcept;

    struct counted_page { T* page; uint32_t live; };
    using page_ref = std::conditional_t<P.free_empty, counted_page, T*>;
    struct split_entry { page_ref zero; page_ref* side; };
    using entry = std::conditional_t<!has_pages, T,
                  std::conditional_t<(zero_bits > 0), split_entry, page_ref>>;

    struct page_record
    {
        T* page = nullptr;
        uint32_t top_index = 0, zero_index = 0;
        bool large = false;
    };

public:
    // tests
    ArrayView<const entry> raw_top() const noexcept;
    ArrayView<const page_record> raw_pages() const noexcept;
    const page_record& raw_spare() const noexcept;
    const superpage_alloc_t& raw_top_alloc() const noexcept;

private:
    page_ref* ref_at(uint32_t top_index, uint32_t zero_index) noexcept requires (has_pages);
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
};

// Levels go outermost first. Inside a level, x takes the lowest bits.
template<typename T, mlt_params P>
constexpr uint64_t multi_level_table<T, P>::pack_coords(coords c) noexcept
{
#ifndef FM_NO_DEBUG
    uint64_t over = 0;
    for (uint32_t d = 0; d < dims; d++)
        over |= uint64_t{c.c[d]} >> dim_bits[d];
    if (over) [[unlikely]]
        bad_coords(c);
#endif
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
