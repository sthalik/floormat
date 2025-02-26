#include "inspect.hpp"
#include "compat/assert.hpp"
#include "compat/defs.hpp"
#include "entity/accessor.hpp"
#include "imgui-raii.hpp"
#include <cstdio>
#include <utility>
#include <Corrade/Containers/StructuredBindings.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Color.h>

namespace floormat::entities {

namespace {

template<typename T> struct IGDT_;
template<> struct IGDT_<uint8_t> : std::integral_constant<int, ImGuiDataType_U8> {};
template<> struct IGDT_<int8_t> : std::integral_constant<int, ImGuiDataType_S8> {};
template<> struct IGDT_<uint16_t> : std::integral_constant<int, ImGuiDataType_U16> {};
template<> struct IGDT_<int16_t> : std::integral_constant<int, ImGuiDataType_S16> {};
template<> struct IGDT_<uint32_t> : std::integral_constant<int, ImGuiDataType_U32> {};
template<> struct IGDT_<int32_t> : std::integral_constant<int, ImGuiDataType_S32> {};
template<> struct IGDT_<uint64_t> : std::integral_constant<int, ImGuiDataType_U64> {};
template<> struct IGDT_<int64_t> : std::integral_constant<int, ImGuiDataType_S64> {};
template<> struct IGDT_<float> : std::integral_constant<int, ImGuiDataType_Float> {};
template<typename T> constexpr auto IGDT = IGDT_<T>::value;

template<typename T> requires std::is_integral_v<T> constexpr bool eqv(T a, T b) { return a == b; }
bool eqv(float a, float b) { return std::fabs(a - b) < 1e-8f; }
bool eqv(const String& a, const String& b) { return a == b; }
template<typename T, size_t N> constexpr bool eqv(const Math::Vector<N, T>& a, const Math::Vector<N, T>& b) { return a == b; }

int on_string_resize(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        auto& str = *static_cast<String*>(data->UserData);
        fm_assert(str.data() == data->Buf);
        if ((size_t)data->BufSize > str.size()+1)
        {
            auto str2 = String{ValueInit, (size_t)data->BufSize};
            str = move(str2);
            data->Buf = str.data();
        }
    }
    return 0;
}

using namespace imgui;

template<typename T>
bool do_inspect_field(void* datum, const erased_accessor& accessor, field_repr repr,
                      const ArrayView<const std::pair<StringView, size_t>>& list,
                      size_t label_width)
{
    if (list.isEmpty())
        fm_assert(accessor.check_field_type<T>());
    fm_assert(!list.isEmpty() == (repr == field_repr::cbx));

    char buf[128];
    bool should_disable = false;

    const auto enabled = accessor.is_enabled(datum);
    fm_assert(enabled < field_status::COUNT);
    switch (enabled)
    {
    using enum field_status;
    case COUNT: fm_assert(false);
    case hidden: return false;
    case readonly: should_disable = true; break;
    case enabled: should_disable = false; break;
    }
    should_disable = should_disable || !accessor.can_write();
    [[maybe_unused]] auto disabler = begin_disabled(should_disable);
    bool ret = false;
    const char* const label = label_left(accessor.field_name, buf, (float)label_width);
    T value{};
    accessor.read_fun(datum, accessor.reader, &value);
    auto orig = value;

    if constexpr(std::is_same_v<T, StringView>)
        ret = ImGui::InputText(label, const_cast<char*>(value.data()), value.size(), ImGuiInputTextFlags_ReadOnly);
    else if constexpr(std::is_same_v<T, String>)
    {
        ret = ImGui::InputText(label, value.begin(), value.size()+1, ImGuiInputTextFlags_CallbackResize, on_string_resize, &value);
        if (ret)
        {
            auto size = value.size();
            if (auto ptr = value.find('\0')) // XXX hack
                size = Math::min(size, (size_t)(ptr.data() - value.data()));
            if (auto max_len = accessor.get_max_length(datum); value.size() > max_len)
                size = Math::min(size, max_len.value);
            if (size != value.size())
                value = value.prefix(size);
        }
    }
    else if constexpr(std::is_same_v<T, bool>)
        ret = ImGui::Checkbox(label, &value);
    else if constexpr (std::is_same_v<T, Color3ub>)
    {
        auto vec = Vector3(value) * (1.f/255);
        ret = ImGui::ColorEdit3(label, vec.data());
        value = Color3ub(vec * 255);
    }
    else if constexpr (std::is_same_v<T, Color4ub>)
    {
        auto vec = Vector4(value) * (1.f/255);
        ret = ImGui::ColorEdit4(label, vec.data());
        value = Color4ub(vec * 255);
    }
    else if constexpr (!Math::IsVector<T>())
    {
        auto [min, max] = accessor.get_range(datum).convert<T>();
        constexpr auto igdt = IGDT<T>;
        constexpr T step(!std::is_floating_point_v<T> ? T(1) : T(1.f)),
                    step2(!std::is_floating_point_v<T> ? T(10) : T(.25f));
        switch (repr)
        {
        default: fm_warn_once("invalid repr enum value '%zu'", (size_t)repr); break;
        case field_repr::input:  ret = ImGui::InputScalar(label, igdt, &value, &step, &step2); break;
        case field_repr::slider: ret = ImGui::SliderScalar(label, igdt, &value, &min, &max); break;
        case field_repr::drag:   ret = ImGui::DragScalar(label, igdt, &value, 1, &min, &max); break;
        case field_repr::cbx: {
            if constexpr(std::is_integral_v<T>)
            {
                StringView preview = "<invalid>"_s;
                const auto old_value = (size_t)static_cast<std::make_unsigned_t<T>>(value);
                for (const auto& [str, x] : list)
                    if (x == old_value)
                    {
                        preview = str;
                        break;
                    }
                if (auto b = begin_combo(label, preview))
                    for (const auto& [str, x] : list)
                    {
                        const bool is_selected = x == (size_t)old_value;
                        if (ImGui::Selectable(str.data(), is_selected))
                            value = T(x), ret = true;
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                break;
            }
        }
        }
        value = Math::clamp(value, min, max);
    }
    else
    {
        using U = typename T::Type;
        auto [min, max] = accessor.get_range(datum).convert<T>();
        constexpr auto igdt = IGDT<U>;
        constexpr T step(!std::is_floating_point_v<U> ? U(1) : U(1.f)),
                    step2(!std::is_floating_point_v<U> ? U(10) : U(0.25f));
        switch (repr)
        {
        default:
            fm_warn_once("invalid repr enum value '%zu'", (size_t)repr);
            break;
        case field_repr::input:
            ret = ImGui::InputScalarN(label, igdt, &value, T::Size, &step, &step2);
            break;
        case field_repr::drag:
            fm_warn_once("can't use imgui input drag mode for vector type");
            [[fallthrough]];
        case field_repr::slider:
            ret = ImGui::SliderScalarN(label, igdt, &value, T::Size, &min, &max);
            break;
        }
        value = Math::clamp(value, min, max);
    }

    if (ret && !should_disable && !eqv(value, orig))
        if (accessor.is_enabled(datum) >= field_status::enabled && accessor.can_write())
        {
            accessor.write_fun(datum, accessor.writer, &value);

            T new_value{};
            accessor.read_fun(datum, accessor.reader, &new_value);
            if (value != new_value)
            {
                auto* state = ImGui::GetInputTextState(GImGui->ActiveId);
                if (state)
                    state->WantReloadUserBuf = true;
            }
            return true;
        }
    return false;
}

} // namespace

#define MAKE_SPEC(type, repr)                                                                                                   \
    template<>                                                                                                                  \
    bool inspect_field<type>(void* datum, const erased_accessor& accessor,                                                      \
                             const ArrayView<const std::pair<StringView, size_t>>& list,                                        \
                             size_t label_width)                                                                                \
    {                                                                                                                           \
        return do_inspect_field<type>(datum, accessor, (repr), list, label_width);                                              \
    }

#define MAKE_SPEC2(type, repr)                                                                                                  \
    template<>                                                                                                                  \
    bool inspect_field<field_repr_<type, field_repr, repr>>(void* datum, const erased_accessor& accessor,                       \
                                                            const ArrayView<const std::pair<StringView, size_t>>& list,         \
                                                            size_t label_width)                                                 \
    {                                                                                                                           \
        return do_inspect_field<type>(datum, accessor, (repr), list, label_width);                                              \
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

MAKE_SPEC_REPRS2(uint8_t)
MAKE_SPEC_REPRS2(int8_t)
MAKE_SPEC_REPRS2(uint16_t)
MAKE_SPEC_REPRS2(int16_t)
MAKE_SPEC_REPRS2(uint32_t)
MAKE_SPEC_REPRS2(int32_t)
MAKE_SPEC_REPRS2(uint64_t)
MAKE_SPEC_REPRS2(int64_t)
MAKE_SPEC_REPRS2(float)
MAKE_SPEC(bool, field_repr::input)
MAKE_SPEC(String, field_repr::input)
MAKE_SPEC(StringView, field_repr::input)
MAKE_SPEC(Color3ub, field_repr::input)
MAKE_SPEC(Color4ub, field_repr::input)

} // namespace floormat::entities
