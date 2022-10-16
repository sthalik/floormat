#pragma once

#include <Corrade/Containers/StringView.h>
#include <Magnum/Math/Color.h>
#include <ImGui.h>

namespace floormat::imgui {

struct raii_wrapper final
{
    using F = void(*)(void);
    raii_wrapper(F fn) : dtor{fn} {}
    raii_wrapper() = default;
    ~raii_wrapper() { if (dtor) dtor(); }
    raii_wrapper(const raii_wrapper&) = delete;
    raii_wrapper& operator=(const raii_wrapper&) = delete;
    raii_wrapper& operator=(raii_wrapper&&) = delete;
    raii_wrapper(raii_wrapper&& other) noexcept : dtor{other.dtor} { other.dtor = nullptr; }
    inline operator bool() const noexcept { return dtor != nullptr; }

    F dtor = nullptr;
};

[[nodiscard]] static inline raii_wrapper begin_window(Containers::StringView name = {},
                                                      ImGuiWindowFlags_ flags = ImGuiWindowFlags_None)
{
    if (name.isEmpty())
        name = "floormat editor";
    if (ImGui::Begin(name.data(), nullptr, flags))
        return {&ImGui::End};
    else
        return {};
}

[[nodiscard]] static inline raii_wrapper begin_main_menu()
{
    if (ImGui::BeginMainMenuBar())
        return {&ImGui::EndMainMenuBar};
    else
        return {};
}
[[nodiscard]] static inline raii_wrapper begin_menu(Containers::StringView name, bool enabled = true)
{
    if (ImGui::BeginMenu(name.data(), enabled))
        return {&ImGui::EndMenu};
    else
        return {};
}

[[nodiscard]] static inline raii_wrapper begin_list_box(Containers::StringView name, ImVec2 size = {})
{
    if (ImGui::BeginListBox(name.data(), size))
        return {&ImGui::EndListBox};
    else
        return {};
}

[[nodiscard]] static inline raii_wrapper tree_node(Containers::StringView name, ImGuiTreeNodeFlags_ flags = ImGuiTreeNodeFlags_None)
{
    if (ImGui::TreeNodeEx(name.data(), flags))
        return {&ImGui::TreePop};
    else
        return {};
}

[[nodiscard]] static inline raii_wrapper push_style_var(ImGuiStyleVar_ var, Vector2 value)
{
    ImGui::PushStyleVar(var, {value[0], value[1]});
    return {[]{ ImGui::PopStyleVar(); }};
}

[[nodiscard]] static inline raii_wrapper push_style_var(ImGuiStyleVar_ var, float value)
{
    ImGui::PushStyleVar(var, value);
    return {[]{ ImGui::PopStyleVar(); }};
}

[[nodiscard]] static inline raii_wrapper push_style_color(ImGuiCol_ var, const Color4& value)
{
    ImGui::PushStyleColor(var, {value[0], value[1], value[2], value[3]});
    return {[]{ ImGui::PopStyleColor(); }};
}

} // namespace floormat::imgui
