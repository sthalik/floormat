#pragma once
#include "multi-level-table-fwd.hpp"
#include "superpage.hpp"
#include <type_traits>
#include <cr/Array.h>

namespace floormat {

// A T that tests false must own nothing: pages are freed without destroying such slots.
template<typename T, mlt_params P>
class multi_level_table final
{
public:
    static constexpr uint32_t key_bits = P.key_bits();
    static constexpr uint32_t page_bits = P.page_bits();
    static constexpr uint32_t zero_bits = P.zero_bits();
    static constexpr uint32_t top_bits = P.top_bits();
    static constexpr bool has_pages = P.has_pages();

    multi_level_table() noexcept;
    ~multi_level_table() noexcept;
    multi_level_table(multi_level_table&& other) noexcept;
    multi_level_table& operator=(multi_level_table&& other) noexcept;

    // the slot may hold T{}
    const T* find(uint64_t key) const noexcept;
    [[nodiscard]] bool insert(uint64_t key, T value) noexcept;
    // the caller destroys the result, after the table is consistent
    [[nodiscard]] T erase(uint64_t key) noexcept;
    void clear() noexcept;
    uint32_t page_count() const noexcept;
    uint64_t size() const noexcept requires (P.free_empty);

private:
    static constexpr size_t top_size = size_t{1} << top_bits;
    static constexpr size_t page_size = size_t{1} << page_bits;
    static constexpr uint32_t page_mask = (1u << page_bits) - 1;
    static constexpr uint32_t zero_mask = (1u << zero_bits) - 1;

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
    void detach(const page_record& rec) noexcept;
    T* add_page(uint32_t top_index, uint32_t zero_index) noexcept;
    void remove_page(T* page) noexcept;
    void recycle(const page_record& rec) noexcept;
    void free_page(const page_record& rec) noexcept;
    void destroy() noexcept;

    entry* _top = nullptr;
    superpage_alloc_t _top_alloc;
    Array<page_record> _pages;
    page_record _spare;
};

} // namespace floormat
