#pragma once
#include "compat/assert.hpp"
#include "compat/enum-constant.hpp"
#include "compat/pretty-function.hpp"
#include <array>
#include <type_traits>
#include <utility>
#include <cr/StringView.h>

namespace floormat::detail::name_of_impl {

constexpr usize my_strlen(const char* str)
{
    const char* start = str;
    for (; *str; str++)
        ;
    return (usize)(str - start);
}

constexpr bool strip_prefix(StringView& s, StringView pfx)
{
    if (s.size() < pfx.size())
        return false;
    for (auto i = 0uz; i < pfx.size(); i++)
        if (s[i] != pfx[i])
            return false;
    s = StringView{ s.data() + pfx.size(), s.size() - pfx.size(), s.flags() };
    return true;
}

constexpr bool strip_suffix(StringView& s, StringView suffix)
{
    if (s.size() < suffix.size())
        return false;
    const auto ssiz = s.size(), sufsiz = suffix.size();
    for (auto i = 0uz; i < sufsiz; i++)
        if (s[ssiz - 1 - i] != suffix[sufsiz - 1 - i])
            return false;
    s = StringView{ s.data(), ssiz - sufsiz, s.flags() };
    return true;
}

} // namespace floormat::detail::name_of_impl

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnrvo"
#endif

template<typename T>
static constexpr auto _fm_internal_type_name_of() // NOLINT(bugprone-reserved-identifier)
{
    using namespace floormat;
    using namespace floormat::detail::name_of_impl;
    using SVF = StringViewFlag;
    constexpr const char* str = FM_PRETTY_FUNCTION;
    auto s = StringView{ str, my_strlen(str), SVF::Global };
#ifdef __CLION_IDE__
    return s;
#endif

    strip_prefix(s, "constexpr "_s);

    if (strip_prefix(s, "auto "_s))
    {
#ifdef _WIN32
        strip_prefix(s, "__cdecl "_s);
#endif
#ifdef _MSC_VER
        if (strip_prefix(s, "_fm_internal_type_name_of<"_s))
        {
            strip_prefix(s, "struct "_s) || strip_prefix(s, "class "_s);
            strip_suffix(s, "(void)"_s);
            bool ret = strip_suffix(s, ">"_s);
            fm_assert(ret);
            // todo s/\b(?:class|struct)\b//g
        }
#endif
#if defined __GNUG__ || defined __clang__
        if (strip_prefix(s, "_fm_internal_type_name_of()"_s))
        {
            if (strip_prefix(s, " [with T = "_s) || strip_prefix(s, " [T = "_s))
            {
                bool ret = strip_suffix(s, "]"_s);
                fm_assert(ret);
            }
        }
#endif
    }
    return s;
}

template<auto value>
static constexpr auto _fm_internal_enumerator_name_of() // NOLINT(bugprone-reserved-identifier)
{
    using namespace floormat;
    using namespace floormat::detail::name_of_impl;
    using SVF = StringViewFlag;
    constexpr const char* str = FM_PRETTY_FUNCTION;
    auto s = StringView{ str, my_strlen(str), SVF::Global };
#ifdef __CLION_IDE__
    return s;
#endif

#ifdef _MSC_VER
    bool ret = strip_prefix(s, "auto __cdecl _fm_internal_enumerator_name_of<"_s) && strip_suffix(s, ">(void)"_s);
#elif defined __clang__
    bool ret = strip_prefix(s, "auto _fm_internal_enumerator_name_of() [value = "_s) && strip_suffix(s, "]"_s);
#else
    bool ret = strip_prefix(s, "constexpr auto _fm_internal_enumerator_name_of() [with auto value = "_s) &&
               strip_suffix(s, "]"_s);
#endif
    fm_assert(ret);
    return s;
}

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

namespace floormat {

template<typename T> constexpr StringView name_of = ::_fm_internal_type_name_of<T>();

template<typename E, E value>
constexpr StringView name_of<enum_constant<E, value>> = ::_fm_internal_enumerator_name_of<value>();

template<typename E, E value>
requires std::is_enum_v<E>
constexpr StringView name_of<std::integral_constant<E, value>> = ::_fm_internal_enumerator_name_of<value>();

template<auto value>
requires std::is_enum_v<decltype(value)>
constexpr inline bool has_enumerator = ::_fm_internal_enumerator_name_of<value>()[0] != '(';

} // namespace floormat

namespace floormat::detail::name_of_impl {

template<auto value>
consteval StringView enumerator_name()
{
    static_assert(has_enumerator<value>, "value has no enumerator");
    constexpr StringView s = ::_fm_internal_enumerator_name_of<value>();
    usize i = s.size();
    while (i > 0 && s[i - 1] != ':')
        i--;
    return StringView{ s.data() + i, s.size() - i, s.flags() };
}

template<typename E, usize... Is>
consteval std::array<StringView, sizeof...(Is)> enumerator_names(std::index_sequence<Is...>)
{
    static_assert((has_enumerator<(E)Is> && ...), "every value below COUNT needs an enumerator");
    return { enumerator_name<(E)Is>()... };
}

} // namespace floormat::detail::name_of_impl

namespace floormat {

// A value with two enumerators gets the name of the one declared first.
template<auto value>
constexpr inline StringView enumerator_name = detail::name_of_impl::enumerator_name<value>();

template<typename E, E COUNT = E::COUNT>
requires std::is_enum_v<E>
constexpr inline auto enumerator_names =
    detail::name_of_impl::enumerator_names<E>(std::make_index_sequence<(usize)COUNT>{});

} // namespace floormat
