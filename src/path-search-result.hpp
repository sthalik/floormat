#pragma once
#include "src/global-coords.hpp"

namespace floormat {

struct path_search_result final
{
    friend class path_search;
    path_search_result();
    size_t size() const;

    const global_coords& operator[](size_t index) const;
    explicit operator ArrayView<global_coords>() const;

    const global_coords* begin() const;
    const global_coords* cbegin() const;
    const global_coords* end() const;
    const global_coords* cend() const;
    const global_coords* data() const;

    explicit operator bool() const;

private:
    static constexpr size_t min_length = TILE_MAX_DIM*2;

    path_search_result* _next = nullptr;
    global_coords* _path = nullptr;
    size_t _size = 0, _reserved = 0;
};

} // namespace floormat
