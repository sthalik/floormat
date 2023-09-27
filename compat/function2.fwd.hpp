#pragma once
//  Copyright 2015-2020 Denis Blank <denis.blank at outlook dot com>
//     Distributed under the Boost Software License, Version 1.0
//       (See accompanying file LICENSE_1_0.txt or copy at
//             http://www.boost.org/LICENSE_1_0.txt)

namespace fu2 {
inline namespace abi_400 {
namespace detail {

template <typename Config, typename Property> class function;
template <bool Owning, bool Copyable, typename Capacity> struct config;
template <bool Throws, bool HasStrongExceptGuarantee, typename... Args> struct property;

} // namespace detail
} // namespace abi_400

struct capacity_default;

template <bool IsOwning, bool IsCopyable, typename Capacity, bool IsThrowing,
          bool HasStrongExceptGuarantee, typename... Signatures>
using function_base = detail::function<
    detail::config<IsOwning, IsCopyable, Capacity>,
    detail::property<IsThrowing, HasStrongExceptGuarantee, Signatures...>>;

template <typename... Signatures>
using function = function_base<true, true, capacity_default, true, false, Signatures...>;

template <typename... Signatures>
using unique_function = function_base<true, false, capacity_default, true, false, Signatures...>;

template <typename... Signatures>
using function_view = function_base<false, true, capacity_default, true, false, Signatures...>;

} // namespace fu2
