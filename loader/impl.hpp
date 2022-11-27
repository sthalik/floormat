#pragma once
#include "loader/loader.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
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

    std::unordered_map<String, std::shared_ptr<struct tile_atlas>> tile_atlas_map;
    std::unordered_map<String, std::shared_ptr<struct anim_atlas>> anim_atlas_map;
    std::vector<String> anim_atlases;

    StringView shader(StringView filename) override;
    Trade::ImageData2D texture(StringView prefix, StringView filename);
    std::shared_ptr<struct tile_atlas> tile_atlas(StringView filename, Vector2ub size) override;
    ArrayView<String> anim_atlas_list() override;
    std::shared_ptr<struct anim_atlas> anim_atlas(StringView name) override;

    void get_anim_atlas_list();

    static void set_application_working_directory();
    static anim_def deserialize_anim(StringView filename);
    static void system_init();
    static bool chdir(StringView pathname);
    [[nodiscard]] static bool check_atlas_name(StringView name);
    void ensure_plugins();

    explicit loader_impl();
    ~loader_impl() override;
};

} // namespace floormat::loader_detail
