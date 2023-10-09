#include "point.hpp"

namespace floormat {

Debug& operator<<(Debug& dbg, const point& pt)
{
    const auto flags = dbg.flags();
    dbg.setFlags(flags | Debug::Flag::NoSpace);

    auto c = Vector3i(chunk_coords_{pt.coord});
    auto t = Vector2i(pt.coord.local());

    dbg << "point( ";
    dbg << c << ", ";
    dbg << t << ", ";
    dbg << pt.offset;
    dbg << " )";

    dbg.setFlags(flags);
    return dbg;
}

} // namespace floormat
