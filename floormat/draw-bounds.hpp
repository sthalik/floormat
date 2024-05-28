#pragma once

namespace floormat {

struct z_bounds final { int8_t min, max, cur; bool only; };
struct draw_bounds final { int16_t minx, maxx, miny, maxy; };

} // namespace floormat
