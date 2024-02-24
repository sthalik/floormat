#pragma once
#include "chunk.hpp"
#include "path-search.hpp"
#include <bitset>

namespace floormat {

struct chunk::pass_region
{
    std::bitset<detail_astar::div_count.product()> bits;
};

} // namespace floormat
