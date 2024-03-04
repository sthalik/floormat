#pragma once

#if defined __clang__ && defined __CLION_IDE__ >= 20240100
#define CORRADE_ASSUME __attribute__((assume(condition)))
#endif

#include "integer-types.hpp"
#include "move.hpp"
#include <type_traits>
#include <Corrade/Tags.h>
#include <Corrade/Utility/Macros.h>
#include <Corrade/Containers/Containers.h>
#include <Magnum/Magnum.h>

// todo add colors prefix thing
#define DBG_nospace (::Corrade::Utility::Debug{::Corrade::Utility::Debug::Flag::NoSpace})
#define WARN_nospace (::Corrade::Utility::Warning{::Corrade::Utility::Debug::Flag::NoSpace})
#define ERR_nospace (::Corrade::Utility::Error{::Corrade::Utility::Debug::Flag::NoSpace})

#define DBG (::Corrade::Utility::Debug{})
#define WARN (::Corrade::Utility::Warning{})
#define ERR (::Corrade::Utility::Error{})

#if defined __CLION_IDE__ || !(defined __cpp_size_t_suffix || defined _MSC_VER && _MSVC_LANG < 202004)
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
    constexpr inline InPlaceInitT InPlace = InPlaceInit;
} // namespace floormat
