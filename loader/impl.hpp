#pragma once
#include "loader/loader.hpp"
#include "compat/safe-ptr.hpp"
#include "compat/borrowed-ptr-fwd.hpp"
#include "atlas-loader-fwd.hpp"
#include <tsl/robin_map.h>
#include <vector>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/StringStlHash.h>
#include <Corrade/Utility/Resource.h>
#include <Corrade/PluginManager/PluginManager.h>
#include <Magnum/Trade/AbstractImporter.h>

namespace floormat::loader_detail {

struct loader_impl final : loader_
{
    explicit loader_impl();
    ~loader_impl() override;
    // >-----> system >----->
    String original_working_directory;

    void set_application_working_directory();
    StringView startup_directory() noexcept override;
    static void system_init();
    void destroy() override;
    static bool chdir(StringView pathname);

    // >-----> plugins >----->
    Optional<PluginManager::Manager<Trade::AbstractImporter>> importer_plugins;
    Containers::Pointer<Trade::AbstractImporter> image_importer;
    Containers::Pointer<Trade::AbstractImporter> tga_importer;

    void ensure_plugins();

    // >-----> resources >----->
    Optional<Utility::Resource> shader_res;
    StringView shader(StringView filename) noexcept override;

    Trade::ImageData2D make_error_texture(Vector2ui size) override;
    Trade::ImageData2D make_error_texture(Vector2ui size, Vector4ub color) override;
    Trade::ImageData2D texture(StringView prefix, StringView filename) noexcept(false) override;

    // >-----> ground >----->
    [[nodiscard]] static atlas_loader<class ground_atlas>* make_ground_atlas_loader();
    safe_ptr<atlas_loader<class ground_atlas>> _ground_loader{ make_ground_atlas_loader() };
    const bptr<class ground_atlas>& ground_atlas(StringView filename, loader_policy policy) noexcept(false) override;
    ArrayView<const ground_cell> ground_atlas_list() noexcept(false) override;
    const ground_cell& invalid_ground_atlas() override;
    bptr<class ground_atlas> get_ground_atlas(StringView name, Vector2ub size, pass_mode pass) noexcept(false) override;

    // >-----> walls >----->
    [[nodiscard]] static atlas_loader<class wall_atlas>* make_wall_atlas_loader();
    safe_ptr<atlas_loader<class wall_atlas>> _wall_loader{ make_wall_atlas_loader() };
    const bptr<class wall_atlas>& wall_atlas(StringView name, loader_policy policy) override;
    ArrayView<const wall_cell> wall_atlas_list() override;
    bptr<class wall_atlas> get_wall_atlas(StringView filename) noexcept(false) override;
    const wall_cell& invalid_wall_atlas() override;

    // >-----> anim >----->
    [[nodiscard]] static atlas_loader<class anim_atlas>* make_anim_atlas_loader();
    safe_ptr<atlas_loader<class anim_atlas>> _anim_loader{ make_anim_atlas_loader() };
    ArrayView<const anim_cell> anim_atlas_list() override;
    bptr<class anim_atlas> anim_atlas(StringView name, StringView dir, loader_policy policy) noexcept(false) override;
    const anim_cell& invalid_anim_atlas() override;
    bptr<class anim_atlas> get_anim_atlas(StringView path) noexcept(false) override;

    // >-----> scenery >----->
    [[nodiscard]] static atlas_loader<struct scenery_proto>* make_scenery_atlas_loader();
    safe_ptr<atlas_loader<struct scenery_proto>> _scenery_loader{ make_scenery_atlas_loader() };
    ArrayView<const scenery_cell> scenery_list() override;
    const struct scenery_proto& scenery(StringView name, loader_policy policy) override;
    const scenery_cell& invalid_scenery_atlas() override;
    struct scenery_proto get_scenery(StringView filename, const scenery_cell& c) noexcept(false) override;

    // >-----> vobjs >----->
    tsl::robin_map<StringView, const struct vobj_cell*> vobj_atlas_map;
    std::vector<struct vobj_cell> vobjs;
    bptr<class anim_atlas> make_vobj_anim_atlas(StringView name, StringView image_filename);
    const struct vobj_cell& vobj(StringView name) override;
    ArrayView<const struct vobj_cell> vobj_list() override;
    void get_vobj_list();
};

} // namespace floormat::loader_detail
