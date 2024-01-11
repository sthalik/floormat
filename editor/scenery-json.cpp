#include "scenery-editor.hpp"
#include "serialize/scenery.hpp"
#include "loader/loader.hpp"
#include "loader/scenery.hpp"
#include <Corrade/Containers/ArrayView.h>

namespace floormat {

void scenery_editor::load_atlases()
{
    _atlases.clear();
    for (const auto& s : loader.sceneries())
        _atlases[s.name] = scenery_{s.name, s.descr, s.proto};
}

} // namespace floormat
