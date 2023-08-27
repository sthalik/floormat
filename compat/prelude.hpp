#pragma once
#include "integer-types.hpp"
#include <type_traits>
#include <Corrade/Containers/Containers.h>
#include <Corrade/Utility/Macros.h>
#include <Magnum/Magnum.h>

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

namespace floormat {
    using namespace ::Magnum;
    using namespace ::Corrade::Containers;
    using namespace ::Corrade::Containers::Literals;
    using namespace ::Magnum::Math::Literals;
    using Debug [[maybe_unused]] = ::Corrade::Utility::Debug;
    using Error [[maybe_unused]] = ::Corrade::Utility::Error;
    namespace Path = Corrade::Utility::Path; // NOLINT(misc-unused-alias-decls)
} // namespace floormat
