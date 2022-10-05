#pragma once
#include <cstddef>
#include <type_traits>

namespace Magnum::Examples {

using size_t = std::size_t;
using ssize_t = std::common_type_t<std::ptrdiff_t, std::make_signed_t<std::size_t>>;

} // namespace Magnum::Examples

#ifdef _MSC_VER
#   define FUNCTION_NAME __FUNCSIG__
#else
#   define FUNCTION_NAME __PRETTY_FUNCTION__
#endif

#define progn(...) [&]{__VA_ARGS__;}()
