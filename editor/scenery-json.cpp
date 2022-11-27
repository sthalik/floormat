#include "scenery-editor.hpp"
#include "serialize/scenery.hpp"
#include "loader/loader.hpp"

namespace floormat {

void scenery_editor::load_atlases()
{
    _atlases.clear();
    for (auto& s : loader.sceneries())
        _atlases[s.name] = scenery_{s.name, s.descr, s.proto};
}

} // namespace floormat
