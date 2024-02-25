#pragma once
#include "chunk.hpp"
#include "search.hpp"
#include <bitset>

namespace floormat {

struct chunk::pass_region
{
    std::bitset<Search::div_count.product()> bits;
};

} // namespace floormat
