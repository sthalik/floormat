#pragma once
#include "integer-types.hpp"
#include <type_traits>
#include <Corrade/Containers/Containers.h>
#include <Corrade/Utility/Macros.h>
#include <Magnum/Magnum.h>

#define DBG_nospace (::Corrade::Utility::Debug{::Corrade::Utility::Debug::Flag::NoSpace})
#define WARNING_nospace (::Corrade::Utility::Warning{::Corrade::Utility::Debug::Flag::NoSpace})
#define ERROR_nospace (::Corrade::Utility::Error{::Corrade::Utility::Debug::Flag::NoSpace})
#define FATAL_nospace (::Corrade::Utility::Fatal{::Corrade::Utility::Debug::Flag::NoSpace})

#define DBG (::Corrade::Utility::Debug{})
#define WARNING (::Corrade::Utility::Warning{})
#define ERROR (::Corrade::Utility::Error{})
#define FATAL (::Corrade::Utility::Fatal{})

#if !(defined __cpp_size_t_suffix || defined _MSC_VER && _MSVC_LANG < 202004)
#ifdef _MSC_VER
#pragma system_header
#else
#pragma GCC system_header
#endif
consteval auto operator""uz(unsigned long long int x) { return ::floormat::size_t(x); }
#endif

namespace Corrade::Containers::Literals {}
namespace Corrade::Utility::Path {}
namespace Magnum::Math::Literals {}

namespace Magnum {
using Vector2uz = ::Magnum::Math::Vector2<size_t>;
using Vector3uz = ::Magnum::Math::Vector3<size_t>;
using Vector4uz = ::Magnum::Math::Vector4<size_t>;
} // namespace Magnum

namespace floormat {
    using namespace ::Magnum;
    using namespace ::Corrade::Containers;
    using namespace ::Corrade::Containers::Literals;
    using namespace ::Magnum::Math::Literals;
    using Debug [[maybe_unused]] = ::Corrade::Utility::Debug;
    using Error [[maybe_unused]] = ::Corrade::Utility::Error;
    namespace Path = Corrade::Utility::Path; // NOLINT(misc-unused-alias-decls)
} // namespace floormat
