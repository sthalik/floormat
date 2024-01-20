#pragma once
#include <cstdio>
#include <concepts>
#include <Corrade/Containers/StringView.h>

namespace floormat::Serialize {

using tilemeta = uint8_t;
using atlasid  = uint32_t;
using chunksiz = uint32_t;
using proto_t  = uint32_t;

template<typename T> struct int_traits;

template<std::unsigned_integral T> struct int_traits<T> { static constexpr T max = T(-1); };
template<std::signed_integral T> struct int_traits<T> { static constexpr T max = T(-1)&~(T(1) << sizeof(T)*8-1); };

namespace {



} // namespace

} // namespace floormat::Serialize

namespace floormat {

namespace {



} // namespace

} // namespace floormat
