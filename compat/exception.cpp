#include "exception.hpp"

namespace floormat {

exception::exception(const exception& other) noexcept
{
    buf.reserve(other.buf.size());
    buf.append(other.buf.begin(), other.buf.end());
}

exception::exception(exception&& other) noexcept = default;

exception& exception::operator=(const exception& other) noexcept
{
    if (&other != this)
    {
        buf.clear();
        buf.reserve(other.buf.size());
        buf.append(other.buf.begin(), other.buf.end());
    }
    return *this;
}

exception& exception::operator=(floormat::exception&&) noexcept = default;

const char* exception::what() const noexcept { return buf.data(); }

} // namespace floormat
