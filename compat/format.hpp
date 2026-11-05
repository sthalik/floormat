#pragma once
#include <fmt/core.h>
#include <cr/Containers.h>

namespace fmt {

// Inherit parse() from the string formatter, or every format spec is a parse error:
// a parse() that consumes nothing leaves the reader on '<' of "{:<44}" instead of '}'.
template<> struct formatter<Corrade::Containers::StringView> : formatter<basic_string_view<char>> {
  auto format(Corrade::Containers::StringView const& s, format_context& ctx) const -> format_context::iterator;
};

template<> struct formatter<Corrade::Containers::String> : formatter<basic_string_view<char>> {
  auto format(Corrade::Containers::String const& s, format_context& ctx) const -> format_context::iterator;
};

} // namespace fmt
