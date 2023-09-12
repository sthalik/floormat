#pragma once
#include <Corrade/Containers/StringView.h>

#if defined _MSC_VER
#define FM_PRETTY_FUNCTION __FUNCSIG__
#else
#define FM_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

template<typename T>
static constexpr auto mangled_name() { // NOLINT(bugprone-reserved-identifier)
    using namespace Corrade::Containers;
    using SVF = StringViewFlag;
    constexpr auto my_strlen = [](const char* str) constexpr -> size_t {
        const char* start = str;
        for (; *str; str++)
            ;
        return (size_t)(str - start);
    };
    const char* str = FM_PRETTY_FUNCTION;
    return StringView { str, my_strlen(str), SVF::Global|SVF::NullTerminated };
}

template<typename T> constexpr inline auto name_of = mangled_name<T>();
