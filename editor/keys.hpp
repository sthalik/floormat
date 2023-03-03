#pragma once

namespace floormat {

enum kmod : int {
    kmod_none  = 0x0000,
    kmod_shift = 0x0001 << 8,
    kmod_ctrl  = 0x0040 << 9,
    kmod_alt   = 0x0100 << 10,
    kmod_super = 0x0400 << 11,
    kmod_mask  = kmod_shift | kmod_ctrl | kmod_alt | kmod_super,
};

enum key : unsigned {
    key_noop,
    key_camera_up, key_camera_left, key_camera_right, key_camera_down, key_camera_reset,
    key_NO_REPEAT,
    key_rotate_tile,
    key_mode_none, key_mode_floor, key_mode_walls, key_mode_scenery,
    key_render_collision_boxes, key_render_clickables,
    key_GLOBAL,
    key_new_file,
    key_quit,
    key_quicksave, key_quickload,
    key_escape,
    key_COUNT, key_MIN = key_noop,
};

} // namespace floormat
