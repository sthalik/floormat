#pragma once

namespace Corrade::Containers::Literals {}
namespace Corrade::Containers {}
namespace Corrade::Utility::Path {}
namespace Corrade::Utility { class Debug; class Error; }
namespace Magnum {}
namespace floormat {
    using namespace ::Magnum;
    using namespace ::Corrade::Containers;
    using namespace ::Corrade::Containers::Literals;
    using Debug [[maybe_unused]] = ::Corrade::Utility::Debug;
    using Error [[maybe_unused]] = ::Corrade::Utility::Error;
    namespace Path = Corrade::Utility::Path; // NOLINT(misc-unused-alias-decls)
} // namespace floormat
