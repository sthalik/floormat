#pragma once
#include "compat/assert.hpp"
#include <cr/StringView.h>

#if defined _MSC_VER
#define FM_PRETTY_FUNCTION __FUNCSIG__
#else
#define FM_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

template<typename T>
static constexpr auto _fm_internal_type_name_of()
{
    // NOLINT(bugprone-reserved-identifier)
    using namespace floormat;
    using SVF = StringViewFlag;
    constexpr auto my_strlen = [](const char* str) constexpr -> size_t {
        const char* start = str;
        for (; *str; str++)
            ;
        return (size_t)(str - start);
    };
    constexpr auto strip_prefix = [](StringView& s, StringView pfx) constexpr {
        if (s.size() < pfx.size())
            return false;
        for (auto i = 0uz; i < pfx.size(); i++)
            if (s[i] != pfx[i])
                return false;
        s = StringView{ s.data() + pfx.size(), s.size() - pfx.size(), s.flags() };
        return true;
    };
    constexpr auto strip_suffix = [](StringView& s, StringView suffix) constexpr {
        if (s.size() < suffix.size())
            return false;
        const auto ssiz = s.size(), sufsiz = suffix.size();
        for (auto i = 0uz; i < sufsiz; i++)
            if (s[ssiz - 1 - i] != suffix[sufsiz - 1 - i])
                return false;
        s = StringView{ s.data(), ssiz - sufsiz, s.flags() };
        return true;
    };
    const char* str = FM_PRETTY_FUNCTION;
    auto s = StringView{ str, my_strlen(str), SVF::Global|SVF::NullTerminated };
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

namespace floormat {

template<typename T> constexpr StringView name_of = ::_fm_internal_type_name_of<T>();

} // namespace floormat
