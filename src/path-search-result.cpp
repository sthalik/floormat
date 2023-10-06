#include "path-search.hpp"
#include "path-search-result.hpp"

namespace floormat {

path_search_result::path_search_result() = default;
size_t path_search_result::size() const { return _size; }

} // namespace floormat
