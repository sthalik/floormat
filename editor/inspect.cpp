#include "inspect.hpp"
#include "compat/assert.hpp"
#include "compat/defs.hpp"
#include "entity/accessor.hpp"
#include "imgui-raii.hpp"
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringIterable.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <algorithm>

namespace floormat::entities {

namespace {

using erased_constraints::is_magnum_vector;

const char* label_left(StringView label, char* buf, std::size_t len)
{
    std::snprintf(buf, len, "##%s", label.data());
    float width = ImGui::CalcItemWidth(), x = ImGui::GetCursorPosX();
    ImGui::Text("%s", label.data());
    ImGui::SameLine();
    ImGui::SetCursorPosX(x + width*.5f + ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(-1);
    return buf;
}

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

template<typename T> requires std::is_integral_v<T> constexpr bool eqv(T a, T b) { return a == b; }
inline bool eqv(float a, float b) { return std::fabs(a - b) < 1e-8f; }
inline bool eqv(const String& a, const String& b) { return a == b; }
template<typename T, std::size_t N> constexpr bool eqv(const Math::Vector<N, T>& a, const Math::Vector<N, T>& b) { return a == b; }

int corrade_string_resize_callback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        auto* my_str = reinterpret_cast<String*>(data->UserData);
        fm_assert(my_str->begin() == data->Buf);
        String tmp = std::move(*my_str);
        *my_str = String{ValueInit, (std::size_t)data->BufSize};
        auto len = std::min(tmp.size(), my_str->size());
        for (std::size_t i = 0; i < len; i++)
            (*my_str)[i] = tmp[i];
        data->Buf = my_str->begin();
    }
    return 0;
}

template<typename T>
void do_inspect_field(void* datum, const erased_accessor& accessor, field_repr repr,
                      const ArrayView<const std::pair<StringView, std::size_t>>& list)
{
    if (list.isEmpty())
        fm_assert(accessor.check_field_type<T>());
    fm_assert(!list.isEmpty() == (repr == field_repr::cbx));

    bool should_disable;
    char buf[128];

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
    const char* const label = label_left(accessor.field_name, buf, sizeof buf);
    T value{};
    accessor.read_fun(datum, accessor.reader, &value);
    auto orig = value;

    if constexpr(std::is_same_v<T, String>)
    {
        ret = ImGui::InputText(label, value.begin(), value.size(), ImGuiInputTextFlags_CallbackResize, corrade_string_resize_callback, &value);
        if (auto max_len = accessor.get_max_length(datum); value.size() > max_len)
            value = value.prefix(max_len);
    }
    else if constexpr(std::is_same_v<T, bool>)
        ret = ImGui::Checkbox(label, &value);
    else if constexpr (!is_magnum_vector<T>)
    {
        auto [min, max] = accessor.get_range(datum).convert<T>();
        constexpr auto igdt = IGDT<T>;
        T step(1), *step_ = !std::is_floating_point_v<T> ? &step : nullptr;
        switch (repr)
        {
        default: fm_warn_once("invalid repr enum value '%zu'", (std::size_t)repr); break;
        case field_repr::input:  ret = ImGui::InputScalar(label, igdt, &value, step_); break;
        case field_repr::slider: ret = ImGui::SliderScalar(label, igdt, &value, &min, &max); break;
        case field_repr::drag:   ret = ImGui::DragScalar(label, igdt, &value, 1, &min, &max); break;
        case field_repr::cbx: {
            if constexpr(std::is_integral_v<T>)
            {
                const char* preview = "<invalid>";
                const auto old_value = (std::size_t)static_cast<std::make_unsigned_t<T>>(value);
                for (const auto& [str, x] : list)
                    if (x == old_value)
                    {
                        preview = str.data();
                        break;
                    }
                if (auto b = begin_combo(label, preview))
                    for (const auto& [str, x] : list)
                    {
                        const bool is_selected = x == (std::size_t)old_value;
                        if (ImGui::Selectable(str.data(), is_selected))
                            value = T(x), ret = true;
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                break;
            }
        }
        }
        value = std::clamp(value, min, max);
    }
    else
    {
        using U = typename T::Type;
        auto [min, max] = accessor.get_range(datum).convert<T>();
        constexpr auto igdt = IGDT<U>;
        T step = T(U(1)), *step_ = !std::is_floating_point_v<U> ? &step : nullptr;
        switch (repr)
        {
        default:
            fm_warn_once("invalid repr enum value '%zu'", (std::size_t)repr);
            break;
        case field_repr::input:
            ret = ImGui::InputScalarN(label, igdt, &value, T::Size, step_);
            break;
        case field_repr::drag:
            fm_warn_once("can't use imgui input drag mode for vector type");
            [[fallthrough]];
        case field_repr::slider:
            ret = ImGui::SliderScalarN(label, igdt, &value, T::Size, &min, &max);
            break;
        }
        for (std::size_t i = 0; i < T::Size; i++)
            value[i] = std::clamp(value[i], min[i], max[i]);
    }

    if (ret && !should_disable && !eqv(value, orig))
        if (accessor.is_enabled(datum) >= field_status::enabled && accessor.can_write())
            accessor.write_fun(datum, accessor.writer, &value);
}

} // namespace

#define MAKE_SPEC(type, repr)                                                                                                   \
    template<>                                                                                                                  \
    void inspect_field<type>(void* datum, const erased_accessor& accessor,                                                      \
                             const ArrayView<const std::pair<StringView, std::size_t>>& list)                                   \
    {                                                                                                                           \
        do_inspect_field<type>(datum, accessor, (repr), list);                                                                  \
    }

#define MAKE_SPEC2(type, repr)                                                                                                  \
    template<>                                                                                                                  \
    void inspect_field<field_repr_<type, field_repr, repr>>(void* datum, const erased_accessor& accessor,                       \
                                                            const ArrayView<const std::pair<StringView, std::size_t>>& list)    \
    {                                                                                                                           \
        do_inspect_field<type>(datum, accessor, (repr), list);                                                                  \
    }

#define MAKE_SPEC_REPRS(type)                                                                                                   \
    MAKE_SPEC2(type, field_repr::input)                                                                                         \
    MAKE_SPEC2(type, field_repr::slider)                                                                                        \
    MAKE_SPEC2(type, field_repr::drag)                                                                                          \
    MAKE_SPEC(type, field_repr::input)

#define MAKE_SPEC_REPRS2(type)                                                                                                  \
    MAKE_SPEC_REPRS(Math::Vector2<type>)                                                                                        \
    MAKE_SPEC_REPRS(Math::Vector3<type>)                                                                                        \
    MAKE_SPEC_REPRS(Math::Vector4<type>)                                                                                        \
    MAKE_SPEC_REPRS(type)                                                                                                       \
    MAKE_SPEC2(type, field_repr::cbx)

MAKE_SPEC_REPRS2(std::uint8_t)
MAKE_SPEC_REPRS2(std::int8_t)
MAKE_SPEC_REPRS2(std::uint16_t)
MAKE_SPEC_REPRS2(std::int16_t)
MAKE_SPEC_REPRS2(std::uint32_t)
MAKE_SPEC_REPRS2(std::int32_t)
MAKE_SPEC_REPRS2(float)
MAKE_SPEC(bool, field_repr::input)
MAKE_SPEC(String, field_repr::input)

} // namespace floormat::entities
