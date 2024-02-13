#include "scenery-editor.hpp"
#include "serialize/scenery.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include <Corrade/Containers/ArrayView.h>

namespace floormat {

void scenery_editor::load_atlases()
{
    _atlases.clear();
    for (const auto& s : loader.scenery_list())
        _atlases[s.name] = { s.name, loader.scenery(s.name) };
}

} // namespace floormat
