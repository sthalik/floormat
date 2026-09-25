#pragma once

namespace floormat::detail {

template<typename T, auto...> struct dependent_typeʹ { using type = T; };

} // namespace floormat::detail

namespace floormat {

// Depends on Xs, so a template body can use T while T is incomplete.
// Clang replaces an alias template `= T` by T at once, so this one goes through a struct.
template<typename T, auto... Xs> using dependent_type = typename detail::dependent_typeʹ<T, Xs...>::type;

} // namespace floormat
