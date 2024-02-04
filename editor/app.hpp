#pragma once
#include "compat/defs.hpp"
#include "compat/enum-bitset-fwd.hpp"
#include "compat/safe-ptr.hpp"
#include "floormat/app.hpp"
#include "keys.hpp"
#include "src/global-coords.hpp"
#include "src/object-id.hpp"
#include "editor-enums.hpp"
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/Optional.h>
#include <Magnum/Math/Vector2.h>

namespace Magnum::GL {
template<UnsignedInt dimensions> class Texture;
typedef Texture<2> Texture2D;
} // namespace Magnum::GL

namespace Magnum::ImGuiIntegration { class Context; }

namespace floormat::wireframe {
GL::Texture2D make_constant_texture();
struct meshes;
} // namespace floormat::wireframe

namespace floormat::tests { struct tests_data; }

namespace floormat {

struct fm_settings;
struct floormat_main;
class world;
class chunk;
class ground_atlas;
class anim_atlas;
struct object;
struct critter;
struct point;
class editor;
template<typename T> struct shared_ptr_wrapper;
struct tests_data_;

struct cursor_state final
{
    Optional<Vector2i> pixel;
    Optional<global_coords> tile;
    Optional<Vector2b> subpixel;
    bool in_imgui = false;

    Optional<struct point> point() const;
};

struct clickable;

enum class Cursor: uint32_t {
    Arrow, TextInput, Wait, Crosshair, WaitArrow,
    ResizeNWSE, ResizeNESW, ResizeWE, ResizeNS, ResizeAll,
    No, Hand, Hidden, HiddenLocked,
};

enum class popup_target_type : unsigned char {
    none, scenery,
};

struct popup_target final {
    object_id id; // todo switch to weak_ptr<object>
    popup_target_type target = popup_target_type::none;
    bool operator==(const popup_target&) const;
};

struct app final : floormat_app
{
    static int run_from_argv(int argc, const char* const* argv);
    static fm_settings parse_cmdline(int argc, const char* const* argv);
    ~app() override;
#ifdef _WIN32
    static void set_dpi_aware();
#endif
    object_id get_object_colliding_with_cursor();
    floormat_main& main();
    const struct cursor_state& cursor_state();
    clickable* find_clickable_scenery(const Optional<Vector2i>& pixel);
    Vector2 point_screen_pos(point pt);
    shared_ptr_wrapper<critter> ensure_player_character(world& w);

private:
    app(fm_settings&& opts);

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(app);
    fm_DECLARE_DEPRECATED_MOVE_ASSIGNMENT(app);

    int exec();

    void update(float dt) override;
    void update_world(float dt);
    void update_cursor_tile(const Optional<Vector2i>& pixel);
    z_bounds get_z_bounds() override;
    void set_cursor();
    void maybe_initialize_chunk(const chunk_coords_& pos, chunk& c) override;
    void maybe_initialize_chunk_(const chunk_coords_& pos, chunk& c);
    void update_character(float dt);
    void reset_world();
    void reset_world(class world&& w);

    void draw() override;

    void on_mouse_move(const mouse_move_event& event) noexcept override;
    void on_mouse_up_down(const mouse_button_event& event, bool is_down) noexcept override;
    void on_mouse_scroll(const mouse_scroll_event& event) noexcept override;
    void on_key_up_down(const key_event& event, bool is_down) noexcept override;
    std::tuple<key, int> resolve_keybinding(int k, int mods);
    void on_text_input_event(const text_input_event& event) noexcept override;
    //bool on_text_editing_event(const text_editing_event& event) noexcept override;
    void on_viewport_event(const Magnum::Math::Vector2<int>& size) noexcept override;
    void on_any_event(const any_event& event) noexcept override;
    void on_focus_in() noexcept override;
    void on_focus_out() noexcept override;
    void on_mouse_leave() noexcept override;
    void on_mouse_enter() noexcept override;

    void do_mouse_move(int modifiers);
    void do_mouse_up_down(uint8_t button, bool is_down, int modifiers);
    void do_mouse_scroll(int offset);

    void do_quicksave();
    void do_quickload();
    void do_new_file();
    void do_escape();

    void draw_collision_boxes();
    void draw_clickables();
    void draw_light_info();
    void draw_lightmap_test(float main_menu_height);
    void do_lightmap_test();

    void draw_editor_pane(float main_menu_height);
    void draw_inspector();
    static void entity_inspector_name(char* buf, size_t len, object_id id);
    static void entity_friendly_name(char* buf, size_t len, const object& obj);
    bool check_inspector_exists(const popup_target& p);
    void set_cursor_from_imgui();
    void draw_cursor();
    void init_imgui(Vector2i size);
    void draw_ui();
    float draw_main_menu();
    void draw_fps();
    void draw_tile_under_cursor();
    void draw_z_level();
    void do_popup_menu();
    void kill_popups(bool hard);
    void render_menu();

    using key_set = enum_bitset<key, key_COUNT>;
    void do_key(key k, int mods);
    void do_key(key k);
    void do_set_mode(editor_mode mode);
    void do_rotate(bool backward);
    void apply_commands(const key_set& k);
    int get_key_modifiers();
    void clear_keys(key min_inclusive, key max_exclusive);
    void clear_keys();
    void clear_non_global_keys();
    void clear_non_repeated_keys();

    void do_camera(float dt, const key_set& cmds, int mods);
    void reset_camera_offset();

    [[nodiscard]] bool tests_handle_key(const key_event& e, bool is_down);
    [[nodiscard]] bool tests_handle_mouse_click(const mouse_button_event& e, bool is_down);
    [[nodiscard]] bool tests_handle_mouse_move(const mouse_move_event& e);
    void tests_pre_update();
    void tests_post_update();
    void draw_tests_pane(float width);
    void draw_tests_overlay();
    void tests_reset_mode();
    tests::tests_data& tests();

    void reserve_inspector_array();
    void add_inspector(popup_target p);
    void erase_inspector(size_t index, ptrdiff_t count = 1);
    void kill_inspectors();

    floormat_main* M;
    safe_ptr<ImGuiIntegration::Context> _imgui;
    safe_ptr<floormat::wireframe::meshes> _wireframe;
    safe_ptr<tests_data_> _tests;
    safe_ptr<editor> _editor;
    safe_ptr<key_set> keys_;
    StaticArray<key_COUNT, int> key_modifiers{ValueInit};
    Array<popup_target> inspectors;
    object_id _character_id = 0;
    struct cursor_state cursor;
    popup_target _popup_target;

    Optional<chunk_coords_> tested_light_chunk;

    int8_t _z_level = 0;

    bool _pending_popup         : 1 = false;
    bool _render_bboxes         : 1 = false;
    bool _render_clickables     : 1 = false;
    bool _render_vobjs          : 1 = true;
    bool _render_all_z_levels   : 1 = true;
};

} // namespace floormat
