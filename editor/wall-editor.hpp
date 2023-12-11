#pragma once
#include "src/rotation.hpp"
#include <memory>
#include <map>

namespace floormat {

class wall_atlas;

class wall_editor
{
    std::map<StringView, std::shared_ptr<wall_atlas>> _atlases;
    std::shared_ptr<wall_atlas> _selected_atlas;
    rotation _r = rotation::N;

public:

    wall_editor();

    enum rotation rotation() const { return _r; }
    void set_rotation(enum rotation r);
    void toggle_rotation();
};

} // namespace floormat
