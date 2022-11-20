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
    return StringView { FM_PRETTY_FUNCTION, SVF::Global|SVF::NullTerminated };
}

template<typename T> constexpr inline auto name_of = mangled_name<T>();
