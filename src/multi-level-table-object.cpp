#include "object-table.hpp"
#include "object.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/multi-level-table.inl"

namespace floormat {

template class multi_level_table<bptr<object>, object_table_params>;

} // namespace floormat
