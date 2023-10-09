#include "point.hpp"

namespace floormat {

Debug& operator<<(Debug& dbg, const point& pt)
{
    const auto flags = dbg.flags();
    dbg.setFlags(flags | Debug::Flag::NoSpace);

    auto c = Vector3i(chunk_coords_{pt.coord});
    auto t = Vector2i(pt.coord.local());
    auto o = pt.offset;

    dbg << "point{";
    dbg << "{" << c.x() << "," << c.y() << "," << c.z() << "},";
    dbg << "{" << t.x() << "," << t.y() << "},";
    dbg << "{" << o.x() << "," << o.y() << "}}";

    dbg.setFlags(flags);
    return dbg;
}

} // namespace floormat
