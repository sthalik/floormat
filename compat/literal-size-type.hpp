#pragma once
#include <cstddef>

namespace floormat {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuser-defined-literals"
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4455)
#endif

constexpr std::size_t operator"" z(unsigned long long int x) noexcept
{
    return std::size_t(x);
}

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined _MSC_VER
#pragma warning(pop)
#endif

} // namespace floormat
