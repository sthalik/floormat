#include "exception.inl"
#include <fmt/format.h>

namespace floormat {

exception::exception(Corrade::Containers::String&& str) noexcept : buf{move(str)} {}

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

namespace floormat::detail {

void vthrow_fmt(fmt::string_view str, fmt::format_args args) noexcept(false)
{
    fmt::memory_buffer out;
    fmt::vformat_to(fmt::appender(out), str, args);
    throw exception{Corrade::Containers::String{out.data(), out.size()}};
}

void throw_soft_assert(const char* expr, const char* file, int line) noexcept(false)
{
    throw_fmt("assertion failed: {} in {}:{}", expr, file, line);
}

} // namespace floormat::detail
