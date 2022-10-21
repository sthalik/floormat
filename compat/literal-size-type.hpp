#pragma once
#if !(defined __cpp_size_t_suffix && __cpp_size_t_suffix >= 202006L)
#include <cstddef>
#include <type_traits>
#include <cstdlib>
#include <limits>

namespace floormat {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuser-defined-literals"
#pragma clang diagnostic ignored "-Wtautological-type-limit-compare"
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4455)
#endif

[[maybe_unused]] constexpr std::size_t operator"" zu(unsigned long long int x) noexcept { if constexpr (sizeof(decltype(x)) > sizeof(std::size_t)) if (x > std::numeric_limits<std::size_t>::max()) std::abort(); return std::size_t(x); }
[[maybe_unused]] constexpr std::size_t operator"" ZU(unsigned long long int x) noexcept { if constexpr (sizeof(decltype(x)) > sizeof(std::size_t)) if (x > std::numeric_limits<std::size_t>::max()) std::abort(); return std::size_t(x); }

[[maybe_unused]] constexpr auto operator"" z(unsigned long long int x) noexcept { if constexpr (sizeof(decltype(x)) >= sizeof(std::size_t)) if (x > (std::size_t)std::numeric_limits<std::make_signed_t<std::size_t>>::max()) std::abort(); return std::make_signed_t<std::size_t>(x); }
[[maybe_unused]] constexpr auto operator"" Z(unsigned long long int x) noexcept { if constexpr (sizeof(decltype(x)) >= sizeof(std::size_t)) if (x > (std::size_t)std::numeric_limits<std::make_signed_t<std::size_t>>::max()) std::abort(); return std::make_signed_t<std::size_t>(x); }

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined _MSC_VER
#pragma warning(pop)
#endif

} // namespace floormat
#endif
