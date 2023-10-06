#pragma once
#include "src/global-coords.hpp"
#include <vector>

namespace floormat {

struct path_search_result final
{
    friend class path_search;

    path_search_result();
    path_search_result(ArrayView<const global_coords> array);
    path_search_result(const path_search_result& other);

    const global_coords* data() const;
    const global_coords& operator[](size_t index) const;
    size_t size() const;

    explicit operator ArrayView<const global_coords>() const;
    explicit operator bool() const;

    const global_coords* begin() const;
    const global_coords* end() const;

private:
    static constexpr size_t min_length = TILE_MAX_DIM*2;

    path_search_result* _next;
    std::vector<global_coords> _path;
};

} // namespace floormat
