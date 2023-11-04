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

namespace floormat { struct anim_def; }

namespace floormat::loader_detail {

struct loader_impl final : loader_
{
    Optional<Utility::Resource> shader_res;
    Optional<PluginManager::Manager<Trade::AbstractImporter>> importer_plugins;
    Containers::Pointer<Trade::AbstractImporter> image_importer;
    Containers::Pointer<Trade::AbstractImporter> tga_importer;

    tsl::robin_map<StringView, std::shared_ptr<struct tile_atlas>> tile_atlas_map;
    tsl::robin_map<StringView, std::shared_ptr<struct anim_atlas>> anim_atlas_map;
    tsl::robin_map<StringView, const struct vobj_info*> vobj_atlas_map;
    std::vector<String> anim_atlases;
    std::vector<struct vobj_info> vobjs;

    std::vector<serialized_scenery> sceneries_array;
    tsl::robin_map<StringView, const serialized_scenery*> sceneries_map;

    String original_working_directory;

    StringView shader(StringView filename) noexcept override;
    Trade::ImageData2D texture(StringView prefix, StringView filename) noexcept(false) override;
    std::shared_ptr<struct tile_atlas> tile_atlas(StringView filename, Vector2ub size, Optional<pass_mode> pass) noexcept(false) override;
    std::shared_ptr<struct tile_atlas> tile_atlas(StringView filename) noexcept(false) override;
    ArrayView<String> anim_atlas_list() override;
    std::shared_ptr<struct anim_atlas> anim_atlas(StringView name, StringView dir) noexcept(false) override;
    const std::vector<serialized_scenery>& sceneries() override;
    const scenery_proto& scenery(StringView name) noexcept(false) override;

    void get_anim_atlas_list();
    void get_scenery_list();
    static anim_def deserialize_anim(StringView filename);

    void get_vobj_list();
    std::shared_ptr<struct anim_atlas> make_vobj_anim_atlas(StringView name, StringView image_filename);
    const struct vobj_info& vobj(StringView name) override;
    ArrayView<struct vobj_info> vobj_list() override;

    void set_application_working_directory();
    StringView startup_directory() noexcept override;
    static void system_init();
    static bool chdir(StringView pathname);
    void ensure_plugins();

    explicit loader_impl();
    ~loader_impl() override;
};

} // namespace floormat::loader_detail
