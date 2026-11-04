#pragma once

namespace floormat {

enum class loader_policy : u8
{
    error, warn, ignore, COUNT, DEFAULT = error,
};

} // namespace floormat
