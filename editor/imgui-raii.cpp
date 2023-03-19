#include "imgui-raii.hpp"
#include "compat/assert.hpp"
#include <Corrade/Containers/StringView.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>

namespace floormat::imgui {

font_saver::~font_saver()
{
    auto& ctx = *ImGui::GetCurrentContext();
    ctx.FontSize = font_size;
    ctx.FontBaseSize = font_base_size;
}

font_saver::font_saver(ImGuiContext& ctx, float size) :
      font_size{ctx.FontSize}, font_base_size{ctx.FontBaseSize}
{
    ctx.FontSize = size;
    ctx.FontBaseSize = size;
}

font_saver::font_saver(float size) : font_saver{*ImGui::GetCurrentContext(), size} {}
style_saver::style_saver() : style{ImGui::GetStyle()} {}
style_saver::~style_saver() { ImGui::GetStyle() = style; }

void text(StringView str, ImGuiTextFlags flags)
{
    ImGui::TextEx(str.data(), str.data() + str.size(), flags);
}

raii_wrapper::raii_wrapper(raii_wrapper::F fn) : dtor{fn} {}
raii_wrapper::~raii_wrapper() { if (dtor) dtor(); }
raii_wrapper::raii_wrapper(raii_wrapper&& other) noexcept : dtor{other.dtor} { other.dtor = nullptr; }
raii_wrapper& raii_wrapper::operator=(raii_wrapper&& other) noexcept
{
    dtor = std::exchange(other.dtor, nullptr);
    return *this;
}
raii_wrapper::operator bool() const noexcept { return dtor != nullptr; }

raii_wrapper push_style_color(ImGuiCol_ var, const Color4& value)
{
    ImGui::PushStyleColor(var, {value[0], value[1], value[2], value[3]});
    return {[]{ ImGui::PopStyleColor(); }};
}

raii_wrapper push_id(StringView str)
{
    ImGui::PushID(str.data(), str.data() + str.size());
    return {[]{ ImGui::PopID(); }};
}

raii_wrapper push_style_var(ImGuiStyleVar_ var, float value)
{
    ImGui::PushStyleVar(var, value);
    return {[]{ ImGui::PopStyleVar(); }};
}

raii_wrapper push_style_var(ImGuiStyleVar_ var, Vector2 value)
{
    ImGui::PushStyleVar(var, {value[0], value[1]});
    return {[]{ ImGui::PopStyleVar(); }};
}

raii_wrapper tree_node(Containers::StringView name, ImGuiTreeNodeFlags flags)
{
    fm_assert(name.flags() & StringViewFlag::NullTerminated);
    if (ImGui::TreeNodeEx(name.data(), flags))
        return {&ImGui::TreePop};
    else
        return {};
}

raii_wrapper begin_disabled(bool is_disabled)
{
    ImGui::BeginDisabled(is_disabled);
    return {&ImGui::EndDisabled};
}

raii_wrapper begin_combo(StringView name, StringView preview, ImGuiComboFlags flags)
{
    fm_assert(name.flags() & StringViewFlag::NullTerminated);
    fm_assert(preview.flags() & StringViewFlag::NullTerminated);
    if (ImGui::BeginCombo(name.data(), preview.data(), flags))
        return {&ImGui::EndCombo};
    else
        return {};
}

raii_wrapper begin_popup(StringView name, ImGuiWindowFlags flags)
{
    fm_assert(name.flags() & StringViewFlag::NullTerminated);
    if (ImGui::BeginPopup(name.data(), flags))
        return {&ImGui::EndPopup};
    else
        return {};
}

raii_wrapper begin_list_box(Containers::StringView name, ImVec2 size)
{
    fm_assert(name.flags() & StringViewFlag::NullTerminated);
    if (ImGui::BeginListBox(name.data(), size))
        return {&ImGui::EndListBox};
    else
        return {};
}

raii_wrapper begin_table(StringView id, int ncols, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width)
{
    fm_assert(id.flags() & StringViewFlag::NullTerminated);
    if (ImGui::BeginTable(id.data(), ncols, flags, outer_size, inner_width))
        return {&ImGui::EndTable};
    else
        return {};
}

raii_wrapper begin_menu(Containers::StringView name, bool enabled)
{
    if (ImGui::BeginMenu(name.data(), enabled))
        return {&ImGui::EndMenu};
    else
        return {};
}

raii_wrapper begin_main_menu()
{
    if (ImGui::BeginMainMenuBar())
        return {&ImGui::EndMainMenuBar};
    else
        return {};
}

raii_wrapper begin_window(Containers::StringView name, bool* p_open, ImGuiWindowFlags flags)
{
    if (name.isEmpty())
        name = "floormat editor";
    if (ImGui::Begin(name.data(), p_open, flags))
        return {&ImGui::End};
    else
        return {};
}

} // namespace floormat::imgui
