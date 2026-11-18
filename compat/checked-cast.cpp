#include "checked-cast.hpp"
#include "exception.hpp"
#include <utility>
#include <fmt/format.h>

namespace fmt {

template<> struct formatter<floormat::detail::checked_cast::erased_number>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    auto format(const floormat::detail::checked_cast::erased_number& x, format_context& ctx) const
        -> format_context::iterator;
};

auto formatter<floormat::detail::checked_cast::erased_number>::format(
    const floormat::detail::checked_cast::erased_number& x, format_context& ctx) const -> format_context::iterator
{
    using enum floormat::detail::checked_cast::erased_number::type_;
    switch (x.type)
    {
    case type_int: return fmt::format_to(ctx.out(), "{}", x.i);
    case type_uint: return fmt::format_to(ctx.out(), "{}", x.u);
    case type_float: return fmt::format_to(ctx.out(), "{}", x.f);
    }
    std::unreachable();
}

} // namespace fmt

namespace floormat::detail::checked_cast {

void throw_out_of_range(erased_number value, erased_number min, erased_number max) noexcept(false)
{
    throw_fmt("checked_cast: {} is out of range [{}, {}]", value, min, max);
}

} // namespace floormat::detail::checked_cast
