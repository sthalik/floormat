#pragma once
#include "defs.hpp"

namespace floormat {

class virtual_memory
{
    void release() noexcept;

    char* _data = nullptr;
    usize _size = 0;

public:
    constexpr virtual_memory() noexcept = default;
    // Takes ownership of bytes reserved at data. On Windows the destructor releases only the allocation at data.
    explicit virtual_memory(char* data, usize bytes) noexcept;
    ~virtual_memory() noexcept;
    virtual_memory(virtual_memory&& other) noexcept;
    virtual_memory& operator=(virtual_memory&& other) noexcept;
    fm_DISABLE_COPY(virtual_memory);

    // Releases the previous reservation.
    void reserve(usize bytes);
    // Rounds out to whole pages. Pages already committed keep their contents.
    void commit(usize offset, usize bytes);
    // offset and bytes must be multiples of page_size().
    // The pages stay inaccessible until the next commit(), which returns them zeroed.
    void decommit(usize offset, usize bytes);

    char* data() const { return _data; }
    usize size() const { return _size; }
    static usize page_size();

    // Returns committed pages holding bytes, at a multiple of alignment, a power of two.
    [[nodiscard]] static char* map_pages(usize bytes, usize alignment);
    // p and bytes must be a result of map_pages() and its argument.
    static void unmap_pages(char* p, usize bytes);
};

} // namespace floormat
