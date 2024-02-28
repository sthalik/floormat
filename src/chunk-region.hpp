#pragma once
#include "chunk.hpp"
#include "search-constants.hpp"
#include <bitset>

namespace floormat {

struct chunk::pass_region
{
    std::bitset<Search::div_count.product()> bits;
    float time = 0;
};

} // namespace floormat
