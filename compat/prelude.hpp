#pragma once

#ifdef _MSC_VER
#if defined _WIN64
typedef unsigned __int64   size_t;
#else
typedef unsigned int       size_t;
#endif
#else
typedef __SIZE_TYPE__      size_t;
#endif

namespace Corrade::Containers {

class String;
template<typename T> class BasicStringView;
using StringView = BasicStringView<const char>;

template<typename T> class ArrayView;

} // namespace Corrade::Containers

namespace Corrade::Containers::Literals {}
namespace Corrade::Utility::Path {}
namespace Corrade::Utility { class Debug; class Error; }
namespace Magnum::Math::Literals {}
namespace Magnum::Math {
template<typename T> class Vector2;
template<typename T> class Vector3;
template<typename T> class Vector4;
} // namespace Magnum::Math
namespace Magnum {
using Vector2uz = Math::Vector2<::size_t>;
using Vector3uz = Math::Vector3<::size_t>;
using Vector4uz = Math::Vector4<::size_t>;
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
