#include "exception.hpp"
#include "compat/format.hpp"

namespace floormat {

exception::exception(detail::exception_tag, fmt::string_view str, fmt::format_args args) noexcept
{
    fmt::memory_buffer out;
    fmt::vformat_to(fmt::appender(out), str, args);
    buf = Corrade::Containers::String{out.data(), out.size()};
}

exception::exception(const exception& other) noexcept : buf{other.buf} {}

exception::exception(exception&& other) noexcept = default;

exception& exception::operator=(const exception& other) noexcept
{
    if (&other != this)
        buf = other.buf;
    return *this;
}

exception& exception::operator=(floormat::exception&&) noexcept = default;

const char* exception::what() const noexcept { return buf.data(); }

} // namespace floormat
