#include "compat/exception.hpp"

namespace floormat {

const char* exception::what() const noexcept { return buf.data(); }

} // namespace floormat
