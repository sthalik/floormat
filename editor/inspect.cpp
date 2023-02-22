#include "inspect.hpp"
#include "compat/assert.hpp"
#include "compat/defs.hpp"
#include "entity/accessor.hpp"
#include "imgui-raii.hpp"
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringIterable.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <algorithm>

namespace floormat {

namespace {

String label_left(StringView label)
{
    float width = ImGui::CalcItemWidth(), x = ImGui::GetCursorPosX();
    ImGui::Text("%s", label.data());
    ImGui::SameLine();
    ImGui::SetCursorPosX(x + width*.5f + ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(-1);
    return ""_s.join(StringIterable({ "##"_s, label }));
}

template<typename T> struct is_magnum_vector_ final : std::false_type {};
template<std::size_t N, typename T> struct is_magnum_vector_<Math::Vector<N, T>> : std::true_type {};
template<typename T> struct is_magnum_vector_<Math::Vector2<T>> : std::true_type {};
template<typename T> struct is_magnum_vector_<Math::Vector3<T>> : std::true_type {};
template<typename T> struct is_magnum_vector_<Math::Vector4<T>> : std::true_type {};
template<typename T> constexpr bool is_magnum_vector = is_magnum_vector_<T>::value;

template<typename T> struct IGDT_;
template<> struct IGDT_<std::uint8_t> : std::integral_constant<int, ImGuiDataType_U8> {};
template<> struct IGDT_<std::int8_t> : std::integral_constant<int, ImGuiDataType_S8> {};
template<> struct IGDT_<std::uint16_t> : std::integral_constant<int, ImGuiDataType_U16> {};
template<> struct IGDT_<std::int16_t> : std::integral_constant<int, ImGuiDataType_S16> {};
template<> struct IGDT_<std::uint32_t> : std::integral_constant<int, ImGuiDataType_U32> {};
template<> struct IGDT_<std::int32_t> : std::integral_constant<int, ImGuiDataType_S32> {};
template<> struct IGDT_<float> : std::integral_constant<int, ImGuiDataType_Float> {};
template<typename T> constexpr auto IGDT = IGDT_<T>::value;

using namespace imgui;
using namespace entities;

template<typename T> void do_inspect_field(void* datum, const erased_accessor& accessor, field_repr repr)
{
    fm_assert(accessor.check_field_name<T>());
    bool should_disable;

    switch (accessor.is_enabled(datum))
    {
    using enum field_status;
    case hidden: return;
    case readonly: should_disable = true; break;
    case enabled: should_disable = false; break;
    }
    should_disable = should_disable || !accessor.can_write();
    [[maybe_unused]] auto disabler = begin_disabled(should_disable);
    bool ret = false;
    const auto label = label_left(accessor.field_name);
    T value{};
    accessor.read_fun(datum, accessor.reader, &value);
    auto orig = value;

    if constexpr (!is_magnum_vector<T>)
    {
        auto [min, max] = accessor.get_range(datum).convert<T>();
        constexpr auto igdt = IGDT<T>;
        switch (repr)
        {
        case field_repr::input:  ret = ImGui::InputScalar(label.data(), igdt, &value); break;
        case field_repr::slider: ret = ImGui::SliderScalar(label.data(), igdt, &value, &min, &max); break;
        case field_repr::drag:   ret = ImGui::DragScalar(label.data(), igdt, &value, 1, &min, &max); break;
        }
        value = std::clamp(value, min, max);
    }
    else
    {
        auto [min_, max_] = accessor.get_range(datum).convert<typename T::Type>();
        Math::Vector<T::Size, typename T::Type> min(min_), max(max_);
        constexpr auto igdt = IGDT<typename T::Type>;
        switch (repr)
        {
        case field_repr::input:
            ret = ImGui::InputScalarN(label.data(), igdt, &value, T::Size);
            break;
        case field_repr::drag:
            fm_warn_once("can't use imgui input drag mode for vector type");
            [[fallthrough]];
        case field_repr::slider:
            ret = ImGui::SliderScalarN(label.data(), igdt, &value, T::Size, &min, &max);
            break;
        }
    }

    ImGui::NewLine();

    if (ret && !should_disable && value != orig)
        if (accessor.is_enabled(datum) >= field_status::enabled && accessor.can_write())
            accessor.write_fun(datum, accessor.writer, &value);
}

} // namespace

#define MAKE_SPEC(type, repr) \
    template<> void inspect_field<type>(void* datum, const erased_accessor& accessor) {                                 \
        do_inspect_field<type>(datum, accessor, (repr));                                                                \
    }

#define MAKE_SPEC_REPR(type, repr)                                                                                      \
    template<> void inspect_field<field_repr_<type, field_repr, repr>>(void* datum, const erased_accessor& accessor) {  \
        do_inspect_field<type>(datum, accessor, (repr));                                                                \
    }

#define MAKE_SPEC_REPRS(type)                   \
    MAKE_SPEC_REPR(type, field_repr::input)     \
    MAKE_SPEC_REPR(type, field_repr::slider)    \
    MAKE_SPEC_REPR(type, field_repr::drag)      \
    MAKE_SPEC(type, field_repr::input)

#define MAKE_SPEC_REPRS2(type)              \
    MAKE_SPEC_REPRS(Math::Vector2<type>)    \
    MAKE_SPEC_REPRS(Math::Vector3<type>)    \
    MAKE_SPEC_REPRS(Math::Vector4<type>)    \
    MAKE_SPEC_REPRS(type)

MAKE_SPEC_REPRS2(std::uint8_t)
MAKE_SPEC_REPRS2(std::int8_t)
MAKE_SPEC_REPRS2(std::uint16_t)
MAKE_SPEC_REPRS2(std::int16_t)
MAKE_SPEC_REPRS2(std::uint32_t)
MAKE_SPEC_REPRS2(std::int32_t)

} // namespace floormat
