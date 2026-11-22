#pragma once
#include "util.hpp"
#include "compat/assert.hpp"
#include "compat/invoke.hpp"
#include <concepts>
#include <type_traits>

namespace floormat::entities {

template<typename F, typename T, typename FieldType>
concept FieldReader = requires(const T& x, F f) {
    { invoke(f, x) } -> std::convertible_to<FieldType>;
};

template<typename F, typename T, typename FieldType>
concept FieldWriter_ptr = std::is_member_object_pointer_v<std::remove_cvref_t<F>> &&
                          requires(T& x, move_qualified<FieldType> value, F f) {
    requires std::is_reference_v<decltype(invoke(f, x))>;
    requires !std::is_const_v<std::remove_reference_t<decltype(invoke(f, x))>>;
    { invoke(f, x) } -> std::convertible_to<FieldType&>;
    { invoke(f, x) = value };
};

template<typename F, typename T, typename FieldType>
concept FieldWriter_invocable = requires(T& x, move_qualified<FieldType> value, F f) {
    { invoke(f, x, value) } -> std::same_as<void>;
};

template<typename F, typename T, typename FieldType>
concept FieldWriter = requires {
    requires FieldWriter_ptr<F, T, FieldType> ||
             FieldWriter_invocable<F, T, FieldType> ||
             std::same_as<F, std::nullptr_t>;
};

} // namespace floormat::entities

namespace floormat::entities::detail {

template<typename Obj, typename Type, FieldReader<Obj, Type> R>
struct read_field {
    static constexpr Type read(const Obj& x, R r) { return invoke(r, x); }
};

template<typename Obj, typename FieldType, FieldWriter<Obj, FieldType> W> struct write_field {
    static constexpr void write(Obj& x, W w, move_qualified<FieldType> value)
    {
        if constexpr (std::is_member_object_pointer_v<W>)
            invoke(w, x) = value;
        else
            invoke(w, x, value);
    }
};

template<typename Obj, typename FieldType>
struct write_field<Obj, FieldType, std::nullptr_t> {
    static constexpr void write(Obj&, std::nullptr_t, move_qualified<FieldType>) { fm_abort("no writing for this accessor"); }
};

} // namespace floormat::entities::detail
