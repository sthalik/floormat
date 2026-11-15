#pragma once
#include "grid-pass.hpp"
#include "search-pred.hpp"
#include "collision.hpp"
#include <cr/ArrayView.h>

namespace floormat::Grid::detail {
struct ObjectGrid;
struct ObjectPool;
} // namespace floormat::Grid::detail

namespace floormat::Grid::Object {

using Params = Pass::Params;

template<typename T>
class cell_object_view
{
    friend detail::ObjectGrid;

    const T* indices;
    const collision_data* entry_data;
    u32 count;

    cell_object_view(const T* indices, const collision_data* entry_data, u32 count);

public:
    class iterator;

    u32 size() const;
    const collision_data& operator[](u32 i) const;
    u32 entry_index(u32 i) const;
    iterator begin() const;
    iterator end() const;
};

template<typename T>
class cell_object_view<T>::iterator
{
    const T* pos;
    const collision_data* entry_data;

public:
    iterator(const T* pos, const collision_data* entry_data);
    const collision_data& operator*() const;
    iterator& operator++();
    bool operator==(const iterator& other) const;
};

class Grid
{
    detail::ObjectGrid* grid;

public:
    ~Grid() noexcept;
    explicit Grid(detail::ObjectGrid* grid);
    Grid(const Grid&) noexcept = default;
    Grid& operator=(const Grid&) & noexcept = default;

    u32 get_cell_index_from_coord(intra_coord pos) const;
    /// @brief The entries a box of bbox_size centred in cell @p index overlaps, valid until the next build.
    template<typename T> cell_object_view<T> objects_at_cell(u32 index) const;
    ArrayView<const collision_data> entry_data() const;
    /// @brief 0 when the entry's object is in this grid's chunk, 1 + n when in world::neighbor_offsets[n]'s.
    ArrayView<const u8> entry_chunks() const;
    /// @brief When true, the T of objects_at_cell() and is_blocked() is u32, else u16.
    bool uses_u32() const;

    /// @brief Writes whether @p p blocks each entry's object to @p entry_blocked.
    /// @return True when it blocks any.
    bool blocked_objects(const Search::pred& p, Array<bool>& entry_blocked) const;
    bool blocked_objects(const Search::pred& p) const;
    template<typename T> bool is_blocked(u32 index, ArrayView<const bool> entry_blocked) const;
    template<typename T> bool is_empty_at(u32 index, u32 count) const;
    /// @brief Writes @p pass_bits to @p out, with each cell cleared that is_blocked(). Both span
    /// detail::bitmask_size(div_count()) bits.
    void mask_pass_bits(ArrayView<const usize> pass_bits, ArrayView<usize> out,
                        ArrayView<const bool> entry_blocked) const;

    u32 div_count() const;
    /// @copydoc floormat::Grid::Pass::Grid::built_pass_gen()
    u64 built_pass_gen() const;
    /// @copydoc floormat::Grid::Pass::Grid::is_marked_stale()
    bool is_marked_stale() const;
    bool is_stale() const;
    /// @copydoc floormat::Grid::Pass::Grid::is_stale(u64)
    bool is_stale(u64 pass_gen) const;
    void mark_stale();
    void build_if_stale();
};

class Pool final
{
    detail::ObjectPool* pool;

public:
    // pass_pool must outlive this pool
    explicit Pool(Pass::Pool& pass_pool);
    ~Pool() noexcept;
    fm_DISABLE_MOVE_COPY(Pool);

    [[nodiscard]] Grid operator[](chunk& c);

    void build_if_stale_all();

    const Params& params() const;
    Pass::Pool& pass_pool() const;
    u32 pooled_count() const;
};

template<typename T>
cell_object_view<T>::cell_object_view(const T* indices, const collision_data* entry_data, u32 count):
    indices{indices}, entry_data{entry_data}, count{count}
{}

template<typename T> u32 cell_object_view<T>::size() const { return count; }

template<typename T> const collision_data& cell_object_view<T>::operator[](u32 i) const
{
    fm_debug3_assert(i < count);
    return entry_data[indices[i]];
}

template<typename T> u32 cell_object_view<T>::entry_index(u32 i) const
{
    fm_debug3_assert(i < count);
    return indices[i];
}

template<typename T> auto cell_object_view<T>::begin() const -> iterator { return {indices, entry_data}; }
template<typename T> auto cell_object_view<T>::end() const -> iterator { return {indices + count, entry_data}; }

template<typename T>
cell_object_view<T>::iterator::iterator(const T* pos, const collision_data* entry_data):
    pos{pos}, entry_data{entry_data}
{}

template<typename T> const collision_data& cell_object_view<T>::iterator::operator*() const
{
    return entry_data[*pos];
}

template<typename T> auto cell_object_view<T>::iterator::operator++() -> iterator&
{
    pos++;
    return *this;
}

template<typename T> bool cell_object_view<T>::iterator::operator==(const iterator& other) const
{
    return pos == other.pos;
}

} // namespace floormat::Grid::Object
