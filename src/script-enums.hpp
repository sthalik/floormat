#pragma once

namespace floormat {

enum class script_lifecycle : u8
{
    no_init, initializing, created, destroying, torn_down, COUNT,
};

enum class script_destroy_reason : u8
{
    quit,       // game is being shut down
    kill,       // object is being deleted from the gameworld
    unassign,   // script is unassigned from object
    COUNT,
};

} // namespace floormat
