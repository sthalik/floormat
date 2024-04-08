#pragma once

namespace floormat {

enum class loader_policy : uint8_t
{
    error, warn, ignore, COUNT, DEFAULT = error,
};

} // namespace floormat
