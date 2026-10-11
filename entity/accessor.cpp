#include "accessor.hpp"

namespace floormat::entities {

bool erased_accessor::can_write() const noexcept { return writer != nullptr; }

} // namespace floormat::entities
