#pragma once

#include "cell.hpp"

// TODO
struct world_data
{
    cell operator()(int x, int y) const;
};
