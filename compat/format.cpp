#include "format.hpp"
#include <fmt/format.h>
#include <cr/StringView.h>

namespace fmt {

auto formatter<Corrade::Containers::StringView>::format(Corrade::Containers::StringView const& s, format_context& ctx) const -> format_context::iterator
{
    return formatter<basic_string_view<char>>::format({s.data(), s.size()}, ctx);
}

auto formatter<Corrade::Containers::String>::format(Corrade::Containers::String const& s, format_context& ctx) const -> format_context::iterator
{
    return formatter<basic_string_view<char>>::format({s.data(), s.size()}, ctx);
}

} // namespace fmt
