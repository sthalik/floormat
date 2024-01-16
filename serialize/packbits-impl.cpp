#include "packbits-impl.hpp"
#include "compat/exception.hpp"

namespace floormat::Pack_impl {

void throw_on_read_nonzero() noexcept(false)
{
    throw std::runtime_error{"extra bits in pack_read()"};
}

} // namespace floormat::Pack_impl
