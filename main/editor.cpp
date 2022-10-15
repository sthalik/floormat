#include "editor.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/tile-atlas.hpp"
#include "src/loader.hpp"
#include <filesystem>
#include <vector>

namespace floormat {

static const std::filesystem::path image_path{IMAGE_PATH, std::filesystem::path::generic_format};

tile_type::tile_type(Containers::StringView name) : _name{name}
{
    load_atlases();
}

void tile_type::load_atlases()
{
    using atlas_array = std::vector<std::shared_ptr<tile_atlas>>;
    for (auto& atlas : json_helper::from_json<atlas_array>(image_path/(_name + ".json")))
    {
        Containers::StringView name = atlas->name();
        if (auto x = name.findLast('.'); x)
            name = name.prefix(x.data());
        _atlases[name] = std::move(atlas);
    }
}

editor_state::editor_state()
{
}

} // namespace floormat
