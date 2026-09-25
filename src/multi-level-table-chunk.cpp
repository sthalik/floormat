#include "chunk-table.hpp"
#include "compat/multi-level-table.inl"

namespace floormat {

template class multi_level_table<chunk*, detail::chunk_table_params>;

} // namespace floormat
