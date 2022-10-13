#pragma once

namespace floormat {

enum class editor_mode : unsigned char {
    select, floors, walls,
};

struct editor_state final
{
    editor_mode mode = {};
    bool dirty = false;
};

} // namespace floormat
