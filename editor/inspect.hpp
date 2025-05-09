#pragma once
#include <utility>
namespace floormat { struct object; }
namespace floormat::entities {

struct erased_accessor;

template<typename T, typename Enum, Enum As>
struct field_repr_ final {
    field_repr_(T x) : value{move(x)} {}
    field_repr_(const field_repr_<T, Enum, As>& other) noexcept = default;
    field_repr_& operator=(T x) noexcept { value = move(x); return *this; }
    field_repr_& operator=(const field_repr_<T, Enum, As>& x) noexcept = default;
    field_repr_& operator*() const noexcept { return value; }
    operator T() const noexcept { return value; }

    using Type = T;
    static constexpr Enum Repr = As;
private:
    T value;
};

enum class field_repr : unsigned char { input, slider, drag, cbx, };

template<typename T> using field_repr_input = field_repr_<T, field_repr, field_repr::input>;
template<typename T> using field_repr_slider = field_repr_<T, field_repr, field_repr::slider>;
template<typename T> using field_repr_drag = field_repr_<T, field_repr, field_repr::drag>;
template<typename T> using field_repr_cbx = field_repr_<T, field_repr, field_repr::cbx>;

bool inspect_object_subtype(object& x);

template<typename T> bool inspect_field(void* datum, const entities::erased_accessor& accessor,
                                        const ArrayView<const std::pair<StringView, size_t>>& list,
                                        size_t label_width);

template<typename T>
requires std::is_enum_v<T>
bool inspect_field(void* datum, const entities::erased_accessor& accessor,
                   const ArrayView<const std::pair<StringView, size_t>>& list,
                   size_t label_width)
{
    return inspect_field<field_repr_cbx<std::underlying_type_t<T>>>(datum, accessor, list, label_width);
}

} // namespace floormat::entities
