#pragma once
#include "loader/loader.hpp"
#include <tsl/robin_map.h>
#include <memory>
#include <vector>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/StringStlHash.h>
#include <Corrade/Utility/Resource.h>
#include <Corrade/PluginManager/PluginManager.h>
#include <Magnum/Trade/AbstractImporter.h>

namespace floormat {
struct anim_def;
struct wall_info;
struct ground_info;
}

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

    Trade::ImageData2D make_error_texture(Vector2ui size);
    Trade::ImageData2D texture(StringView prefix, StringView filename) noexcept(false) override;

    // >-----> walls >----->
    tsl::robin_map<StringView, wall_info*> wall_atlas_map;
    std::vector<wall_info> wall_atlas_array;
    std::vector<String> missing_wall_atlases;
    Pointer<wall_info> invalid_wall_atlas;
    std::shared_ptr<class wall_atlas> wall_atlas(StringView name, bool fail_ok = true) override;
    ArrayView<const wall_info> wall_atlas_list() override;
    void get_wall_atlas_list();
    const wall_info& make_invalid_wall_atlas();
    std::shared_ptr<class wall_atlas> get_wall_atlas(StringView name, StringView dir);

    // >-----> tile >----->
    tsl::robin_map<StringView, ground_info*> ground_atlas_map;
    std::vector<ground_info> ground_atlas_array;
    std::vector<String> missing_ground_atlases;
    Pointer<ground_info> invalid_ground_atlas;
    std::shared_ptr<class ground_atlas> ground_atlas(StringView filename, bool fail_ok) noexcept(false) override;
    ArrayView<const ground_info> ground_atlas_list() noexcept(false) override;
    void get_ground_atlas_list();
    const ground_info& make_invalid_ground_atlas();
    std::shared_ptr<class ground_atlas> get_ground_atlas(StringView name, Vector2ub size, pass_mode pass) noexcept(false) override;

    // >-----> anim >----->
    tsl::robin_map<StringView, std::shared_ptr<class anim_atlas>> anim_atlas_map;
    std::vector<String> anim_atlases;
    ArrayView<const String> anim_atlas_list() override;
    std::shared_ptr<class anim_atlas> anim_atlas(StringView name, StringView dir) noexcept(false) override;
    static anim_def deserialize_anim(StringView filename);
    void get_anim_atlas_list();

    // >-----> scenery >----->
    std::vector<serialized_scenery> sceneries_array;
    tsl::robin_map<StringView, const serialized_scenery*> sceneries_map;
    ArrayView<const serialized_scenery> sceneries() override;
    const scenery_proto& scenery(StringView name) noexcept(false) override;
    void get_scenery_list();

    // >-----> vobjs >----->
    tsl::robin_map<StringView, const struct vobj_info*> vobj_atlas_map;
    std::vector<struct vobj_info> vobjs;
    std::shared_ptr<class anim_atlas> make_vobj_anim_atlas(StringView name, StringView image_filename);
    const struct vobj_info& vobj(StringView name) override;
    ArrayView<const struct vobj_info> vobj_list() override;
    void get_vobj_list();
};

} // namespace floormat::loader_detail
