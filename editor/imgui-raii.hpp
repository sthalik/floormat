#pragma once
#include "compat/prelude.hpp"
#include <Corrade/Containers/StringView.h>
#include <Magnum/Magnum.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace floormat::imgui {

struct raii_wrapper final
{
    using F = void(*)(void);
    raii_wrapper(F fn);
    raii_wrapper() = default;
    ~raii_wrapper();
    raii_wrapper(const raii_wrapper&) = delete;
    raii_wrapper& operator=(const raii_wrapper&) = delete;
    raii_wrapper& operator=(raii_wrapper&&) = delete;
    raii_wrapper(raii_wrapper&& other) noexcept;
    operator bool() const noexcept;

private:
    F dtor = nullptr;
};

[[nodiscard]] raii_wrapper begin_window(StringView name = {}, ImGuiWindowFlags_ flags = ImGuiWindowFlags_None);
[[nodiscard]] raii_wrapper begin_main_menu();
[[nodiscard]] raii_wrapper begin_menu(StringView name, bool enabled = true);
[[nodiscard]] raii_wrapper begin_list_box(StringView name, ImVec2 size = {});
[[nodiscard]] raii_wrapper tree_node(StringView name, ImGuiTreeNodeFlags_ flags = ImGuiTreeNodeFlags_None);
[[nodiscard]] raii_wrapper push_style_var(ImGuiStyleVar_ var, Vector2 value);
[[nodiscard]] raii_wrapper push_style_var(ImGuiStyleVar_ var, float value);
[[nodiscard]] raii_wrapper push_style_color(ImGuiCol_ var, const Color4& value);
void text(const char* str, std::size_t len, ImGuiTextFlags_ flags = ImGuiTextFlags_NoWidthForLargeClippedText);

template<std::size_t N>
void text(const char (&buf)[N], ImGuiTextFlags_ flags = ImGuiTextFlags_NoWidthForLargeClippedText)
{
    ImGui::TextEx(buf, buf + N - 1, flags);
}

struct style_saver final
{
    style_saver();
    ~style_saver();
private:
    ImGuiStyle style;
};

struct font_saver final
{
    font_saver(float size);
    ~font_saver();
private:
    font_saver(ImGuiContext& ctx, float size);

    float font_size, font_base_size;
};

} // namespace floormat::imgui
