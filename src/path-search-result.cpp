#include "path-search.hpp"
#include "path-search-result.hpp"
#include "compat/assert.hpp"

namespace floormat {

path_search_result::path_search_result() : _next{nullptr} {}
size_t path_search_result::size() const { return _path.size(); }
path_search_result::operator bool() const { return !_path.empty(); }
path_search_result::operator ArrayView<const global_coords>() const { return {_path.data(), _path.size()}; }
const global_coords* path_search_result::begin() const { return _path.data(); }
const global_coords* path_search_result::end() const { return _path.data() + _path.size(); }

const global_coords& path_search_result::operator[](size_t index) const
{
    fm_debug_assert(index < _path.size());
    return data()[index];
}

const global_coords* path_search_result::data() const
{
    fm_debug_assert(!_next);
    return _path.data();
}

path_search_result::path_search_result(ArrayView<const global_coords> array) : _next{nullptr}
{
    _path.reserve(std::max(array.size(), min_length));
    _path = {array.begin(), array.end()};
}

path_search_result::path_search_result(const path_search_result& other) : _next{nullptr}
{
    _path.reserve(std::max(min_length, other._path.size()));
    _path = {other._path.begin(), other._path.end()};
}

} // namespace floormat
