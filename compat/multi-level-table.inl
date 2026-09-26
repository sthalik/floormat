#pragma once
#include "multi-level-table.hpp"
#include "compat/assert.hpp"
#include "entity/name-of.hpp"
#include <memory>
#include <utility>
#include <cr/GrowableArray.h>

namespace floormat::detail_mlt {

constexpr size_t superpage_size = size_t{2} << 20;

template<typename Table>
CORRADE_ALWAYS_INLINE void check_key(uint64_t key) noexcept
{
#ifndef FM_NO_DEBUG
    if constexpr (Table::key_bits < 64)
        if (key >> Table::key_bits) [[unlikely]]
            fm_abort("key 0x%llx out of bounds for %.*s", (unsigned long long)key,
                     (int)name_of<Table>.size(), name_of<Table>.data());
#else
    (void)key;
#endif
}

template<typename T> CORRADE_ALWAYS_INLINE T* page_of(T* ref) noexcept { return ref; }
template<typename Ref> CORRADE_ALWAYS_INLINE auto page_of(const Ref& ref) noexcept { return ref.page; }

} // namespace floormat::detail_mlt

namespace floormat {

template<typename T, mlt_params P>
multi_level_table<T, P>::multi_level_table() noexcept
{
    static_assert(std::is_nothrow_default_constructible_v<T>);
    static_assert(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>);
    static_assert(std::is_constructible_v<bool, const T&>);
    static_assert(P.top_source == mlt_source::heap || sizeof(entry) * top_size % detail_mlt::superpage_size == 0);
    static_assert(P.page_source == mlt_source::heap || sizeof(T) * page_size % detail_mlt::superpage_size == 0);

    if constexpr (P.top_source == mlt_source::superpage)
    {
        _top_alloc = superpage_alloc(sizeof(entry) * top_size);
        _top = static_cast<entry*>(_top_alloc.ptr);
        if constexpr (!std::is_trivially_default_constructible_v<entry>)
            std::uninitialized_value_construct_n(_top, top_size);
    }
    else
        _top = new entry[top_size]{};
}

template<typename T, mlt_params P>
multi_level_table<T, P>::~multi_level_table() noexcept
{
    destroy();
}

template<typename T, mlt_params P>
multi_level_table<T, P>::multi_level_table(multi_level_table&& other) noexcept :
    _top{other._top}, _top_alloc{other._top_alloc}, _pages{move(other._pages)}, _spare{other._spare}
{
    other._top = nullptr;
    other._top_alloc = {};
    other._spare = {};
}

template<typename T, mlt_params P>
auto multi_level_table<T, P>::operator=(multi_level_table&& other) noexcept -> multi_level_table&
{
    fm_debug_assert(&other != this);
    destroy();
    _top = other._top;
    _top_alloc = other._top_alloc;
    _pages = move(other._pages);
    _spare = other._spare;
    other._top = nullptr;
    other._top_alloc = {};
    other._spare = {};
    return *this;
}

template<typename T, mlt_params P>
const T* multi_level_table<T, P>::find(uint64_t key) const noexcept
{
    detail_mlt::check_key<multi_level_table>(key);
    if constexpr (!has_pages)
        return &_top[key];
    else
    {
        const entry& e = _top[key >> (page_bits + zero_bits)];
        const T* page;
        if constexpr (zero_bits > 0)
        {
            const auto z = uint32_t(key >> page_bits) & zero_mask;
            if (!z)
                page = detail_mlt::page_of(e.zero);
            else if (!e.side)
                return nullptr;
            else
                page = detail_mlt::page_of(e.side[z]);
        }
        else
            page = detail_mlt::page_of(e);
        if (!page)
            return nullptr;
        return &page[uint32_t(key) & page_mask];
    }
}

template<typename T, mlt_params P>
bool multi_level_table<T, P>::insert(uint64_t key, T value) noexcept
{
    detail_mlt::check_key<multi_level_table>(key);
    T* slot;
    [[maybe_unused]] page_ref* ref = nullptr;
    if constexpr (!has_pages)
        slot = &_top[key];
    else
    {
        const auto ti = uint32_t(key >> (page_bits + zero_bits));
        entry& e = _top[ti];
        uint32_t zi = 0;
        if constexpr (zero_bits > 0)
        {
            zi = uint32_t(key >> page_bits) & zero_mask;
            if (!zi)
                ref = &e.zero;
            else
            {
                if (!e.side)
                    e.side = new page_ref[size_t{1} << zero_bits]{};
                ref = &e.side[zi];
            }
        }
        else
            ref = &e;
        T* page = detail_mlt::page_of(*ref);
        if (!page)
        {
            page = add_page(ti, zi);
            if constexpr (P.free_empty)
                ref->page = page;
            else
                *ref = page;
        }
        slot = &page[uint32_t(key) & page_mask];
    }
    if (*slot)
        return false;
    *slot = move(value);
    if constexpr (P.free_empty)
        ref->live++;
    return true;
}

template<typename T, mlt_params P>
T multi_level_table<T, P>::erase(uint64_t key) noexcept
{
    detail_mlt::check_key<multi_level_table>(key);
    if constexpr (!has_pages)
        return std::exchange(_top[key], T{});
    else
    {
        entry& e = _top[key >> (page_bits + zero_bits)];
        page_ref* ref;
        if constexpr (zero_bits > 0)
        {
            const auto z = uint32_t(key >> page_bits) & zero_mask;
            if (!z)
                ref = &e.zero;
            else if (!e.side)
                return T{};
            else
                ref = &e.side[z];
        }
        else
            ref = &e;
        T* page = detail_mlt::page_of(*ref);
        if (!page)
            return T{};
        T ret = std::exchange(page[uint32_t(key) & page_mask], T{});
        if constexpr (P.free_empty)
            if (ret && !--ref->live)
            {
                ref->page = nullptr;
                remove_page(page);
            }
        return ret;
    }
}

template<typename T, mlt_params P>
void multi_level_table<T, P>::clear() noexcept
{
    if constexpr (!has_pages)
    {
        for (size_t i = 0; i < top_size; i++)
            (void)std::exchange(_top[i], T{});
    }
    else
    {
        // A T destructor that re-enters the table must find every page gone.
        Array<page_record> pages = move(_pages);
        for (const page_record& rec : pages)
            detach(rec);
        for (const page_record& rec : pages)
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
                for (size_t i = 0; i < page_size; i++)
                    if (rec.page[i])
                        (void)std::exchange(rec.page[i], T{});
            free_page(rec);
        }
        if constexpr (zero_bits > 0)
            for (const page_record& rec : pages)
            {
                split_entry& e = _top[rec.top_index];
                delete[] e.side;
                e.side = nullptr;
            }
        if (_pages.isEmpty())
        {
            arrayClear(pages);
            _pages = move(pages);
        }
    }
}

template<typename T, mlt_params P>
uint32_t multi_level_table<T, P>::page_count() const noexcept
{
    return (uint32_t)_pages.size();
}

template<typename T, mlt_params P>
uint64_t multi_level_table<T, P>::size() const noexcept requires (P.free_empty)
{
    uint64_t n = 0;
    for (const page_record& rec : _pages)
    {
        const entry& e = _top[rec.top_index];
        if constexpr (zero_bits > 0)
            n += (rec.zero_index ? e.side[rec.zero_index] : e.zero).live;
        else
            n += e.live;
    }
    return n;
}

template<typename T, mlt_params P>
void multi_level_table<T, P>::detach(const page_record& rec) noexcept
{
    if constexpr (has_pages)
    {
        entry& e = _top[rec.top_index];
        page_ref* ref;
        if constexpr (zero_bits > 0)
            ref = rec.zero_index ? &e.side[rec.zero_index] : &e.zero;
        else
            ref = &e;
        *ref = page_ref{};
    }
}

template<typename T, mlt_params P>
T* multi_level_table<T, P>::add_page(uint32_t top_index, uint32_t zero_index) noexcept
{
    page_record rec = _spare;
    _spare = {};
    if (!rec.page)
    {
        if constexpr (P.page_source == mlt_source::superpage)
        {
            const auto a = superpage_alloc(sizeof(T) * page_size);
            rec.page = static_cast<T*>(a.ptr);
            rec.large = a.used_large;
            if constexpr (!std::is_trivially_default_constructible_v<T>)
                std::uninitialized_value_construct_n(rec.page, page_size);
        }
        else
            rec.page = new T[page_size]{};
    }
    rec.top_index = top_index;
    rec.zero_index = zero_index;
    arrayAppend(_pages, rec);
    return rec.page;
}

template<typename T, mlt_params P>
void multi_level_table<T, P>::remove_page(T* page) noexcept
{
    for (size_t i = _pages.size(); i-- > 0; )
        if (_pages[i].page == page)
        {
            const page_record rec = _pages[i];
            arrayRemoveUnordered(_pages, i);
            recycle(rec);
            return;
        }
    fm_abort("page %p missing from %.*s", (const void*)page,
             (int)name_of<multi_level_table>.size(), name_of<multi_level_table>.data());
}

template<typename T, mlt_params P>
void multi_level_table<T, P>::recycle(const page_record& rec) noexcept
{
    if constexpr (P.free_empty)
        if (!_spare.page)
        {
            _spare = rec;
            return;
        }
    free_page(rec);
}

template<typename T, mlt_params P>
void multi_level_table<T, P>::free_page(const page_record& rec) noexcept
{
    if constexpr (P.page_source == mlt_source::superpage)
        superpage_free({rec.page, sizeof(T) * page_size, rec.large});
    else
        delete[] rec.page;
}

template<typename T, mlt_params P>
void multi_level_table<T, P>::destroy() noexcept
{
    if (!_top)
        return;
    clear();
    if (_spare.page)
        free_page(_spare);
    _spare = {};
    if constexpr (P.top_source == mlt_source::superpage)
        superpage_free(_top_alloc);
    else
        delete[] _top;
    _top = nullptr;
    _top_alloc = {};
}

} // namespace floormat
