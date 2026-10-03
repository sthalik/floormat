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

template<typename T>
CORRADE_ALWAYS_INLINE bool occupied(const T& x) noexcept
{
    if constexpr (requires { x.has_block(); })
        return x.has_block();
    else
        return bool(x);
}

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
    fm_assert(!other._clearing);
    other._top = nullptr;
    other._top_alloc = {};
    other._spare = {};
}

template<typename T, mlt_params P>
auto multi_level_table<T, P>::operator=(multi_level_table&& other) noexcept -> multi_level_table&
{
    fm_debug_assert(&other != this);
    fm_assert(!other._clearing);
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
    fm_debug_assert(detail_mlt::occupied(value));
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
                    e.side = new page_ref[1u << zero_bits]{};
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
            {
                ref->page = page;
                // side[0] is never a page: its count is the side array's pages
                if constexpr (zero_bits > 0)
                    if (zi)
                        e.side[0].live++;
            }
            else
                *ref = page;
        }
        slot = &page[uint32_t(key) & page_mask];
    }
    if (detail_mlt::occupied(*slot))
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
        const auto ti = uint32_t(key >> (page_bits + zero_bits));
        entry& e = _top[ti];
        uint32_t zi = 0;
        page_ref* ref;
        if constexpr (zero_bits > 0)
        {
            zi = uint32_t(key >> page_bits) & zero_mask;
            if (!zi)
                ref = &e.zero;
            else if (!e.side)
                return T{};
            else
                ref = &e.side[zi];
        }
        else
            ref = &e;
        T* page = detail_mlt::page_of(*ref);
        if (!page)
            return T{};
        T ret = std::exchange(page[uint32_t(key) & page_mask], T{});
        if constexpr (P.free_empty)
            if (detail_mlt::occupied(ret))
            {
                const uint32_t live = --ref->live;
                fm_debug2_assert(live != (uint32_t)-1);
                if (!live)
                    release_page(ti, zi, page);
            }
        return ret;
    }
}

template<typename T, mlt_params P>
const T* multi_level_table<T, P>::find(uint32_t x, uint32_t y, uint32_t z) const noexcept requires (dims == 3)
{
    return find(pack(x, y, z));
}

template<typename T, mlt_params P>
bool multi_level_table<T, P>::insert(uint32_t x, uint32_t y, uint32_t z, T value) noexcept requires (dims == 3)
{
    return insert(pack(x, y, z), move(value));
}

template<typename T, mlt_params P>
T multi_level_table<T, P>::erase(uint32_t x, uint32_t y, uint32_t z) noexcept requires (dims == 3)
{
    return erase(pack(x, y, z));
}

template<typename T, mlt_params P>
void multi_level_table<T, P>::clear() noexcept
{
    fm_assert(!_clearing);
    _clearing = true;
    // Values go one at a time, so each destructor sees a consistent table. It may insert
    // anywhere, so every loop runs until nothing is left.
    if constexpr (!has_pages)
    {
        for (bool again = true; again; )
        {
            again = false;
            for (uint32_t i = 0; i < top_size; i++)
                if (detail_mlt::occupied(_top[i]))
                {
                    (void)std::exchange(_top[i], T{});
                    again = !std::is_trivially_destructible_v<T>;
                }
        }
    }
    else if constexpr (std::is_trivially_destructible_v<T>)
        free_all_pages();
    else if constexpr (P.free_empty)
    {
        // every page in _pages holds a value, so each pass destroys one
        while (!_pages.isEmpty())
        {
            const page_record rec = _pages.back();
            bool destroyed = false;
            for (uint32_t i = 0; i < page_size; i++)
            {
                // a destructor may have freed the page, or the side array holding its ref
                page_ref* ref = ref_at(rec.top_index, rec.zero_index);
                if (!ref || ref->page != rec.page)
                    break;
                if (!detail_mlt::occupied(rec.page[i]))
                    continue;
                destroyed = true;
                T value = std::exchange(rec.page[i], T{});
                const uint32_t live = --ref->live;
                fm_debug2_assert(live != (uint32_t)-1);
                if (!live)
                    release_page(rec.top_index, rec.zero_index, rec.page);
            }
            fm_assert(destroyed);
        }
    }
    else
    {
        // erase() never frees a page here, and add_page() appends
        for (bool again = true; again; )
        {
            again = false;
            for (uint32_t p = 0; p < _pages.size(); p++)
                for (uint32_t i = 0; i < page_size; i++)
                {
                    T* page = _pages[p].page;
                    if (detail_mlt::occupied(page[i]))
                    {
                        (void)std::exchange(page[i], T{});
                        again = true;
                    }
                }
        }
        free_all_pages();
    }
    _clearing = false;
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
auto multi_level_table<T, P>::raw_top() const noexcept -> ArrayView<const entry>
{
    return {_top, _top ? top_size : 0};
}

template<typename T, mlt_params P>
auto multi_level_table<T, P>::raw_pages() const noexcept -> ArrayView<const page_record>
{
    return _pages;
}

template<typename T, mlt_params P>
auto multi_level_table<T, P>::raw_spare() const noexcept -> const page_record&
{
    return _spare;
}

template<typename T, mlt_params P>
const superpage_alloc_t& multi_level_table<T, P>::raw_top_alloc() const noexcept
{
    return _top_alloc;
}

template<typename T, mlt_params P>
void multi_level_table<T, P>::bad_coords(coords c) noexcept
{
    fm_abort("coordinates (%u, %u, %u) out of bounds for %.*s", c.c[0], c.c[1], c.c[2],
             (int)name_of<multi_level_table>.size(), name_of<multi_level_table>.data());
}

template<typename T, mlt_params P>
auto multi_level_table<T, P>::ref_at(uint32_t top_index, uint32_t zero_index) noexcept -> page_ref* requires (has_pages)
{
    entry& e = _top[top_index];
    if constexpr (zero_bits > 0)
    {
        if (!zero_index)
            return &e.zero;
        return e.side ? &e.side[zero_index] : nullptr;
    }
    else
        return &e;
}

template<typename T, mlt_params P>
void multi_level_table<T, P>::detach(const page_record& rec) noexcept
{
    if constexpr (has_pages)
        *ref_at(rec.top_index, rec.zero_index) = page_ref{};
}

template<typename T, mlt_params P>
void multi_level_table<T, P>::release_page(uint32_t top_index, uint32_t zero_index, T* page) noexcept requires (P.free_empty)
{
    *ref_at(top_index, zero_index) = page_ref{};
    if constexpr (zero_bits > 0)
        if (zero_index)
        {
            split_entry& e = _top[top_index];
            const uint32_t live = --e.side[0].live;
            fm_debug2_assert(live != (uint32_t)-1);
            if (!live)
            {
                delete[] e.side;
                e.side = nullptr;
            }
        }
    remove_page(page);
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
    for (auto i = (uint32_t)_pages.size(); i-- > 0; )
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
void multi_level_table<T, P>::free_all_pages() noexcept
{
    for (const page_record& rec : _pages)
        detach(rec);
    if constexpr (zero_bits > 0)
        for (const page_record& rec : _pages)
        {
            split_entry& e = _top[rec.top_index];
            delete[] e.side;
            e.side = nullptr;
        }
    // the pages may hold trivially destructible values, so none may become the spare
    for (const page_record& rec : _pages)
        free_page(rec);
    arrayClear(_pages);
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
