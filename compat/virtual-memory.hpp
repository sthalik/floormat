#pragma once
#include "defs.hpp"

namespace floormat {

class virtual_memory
{
    void release() noexcept;

    char* _data = nullptr;
    usize _size = 0;

public:
    virtual_memory() noexcept;
    ~virtual_memory() noexcept;
    virtual_memory(virtual_memory&& other) noexcept;
    virtual_memory& operator=(virtual_memory&& other) noexcept;
    fm_DISABLE_COPY(virtual_memory);

    // Releases the previous reservation.
    void reserve(usize bytes);
    // Rounds out to whole pages. Pages already committed keep their contents.
    void commit(usize offset, usize bytes);

    char* data() const { return _data; }
    usize size() const { return _size; }
    static usize page_size();
};

} // namespace floormat
