#include "point.hpp"
#include "compat/int-hash.hpp"

namespace floormat {

size_t point::hash() const
{
    constexpr size_t size = 2 * 2 + 1 + 1 + 2;
    static_assert(sizeof *this == size);
#ifdef FLOORMAT_64
    static_assert(sizeof nullptr > 4);
    return fnvhash_64(this, sizeof *this);
#else
    static_assert(sizeof nullptr == 4);
    return fnvhash_32(this, sizeof *this);
#endif
}

Debug& operator<<(Debug& dbg, const point& pt)
{
    dbg << "";
    const auto flags = dbg.flags();
    dbg.setFlags(flags | Debug::Flag::NoSpace);

    auto c = Vector3i(pt.chunk3());
    auto t = Vector2i(pt.local());
    auto o = pt.offset();

    dbg << "point{";
    dbg << "{" << c.x() << "," << c.y() << "," << c.z() << "},";
    dbg << "{" << t.x() << "," << t.y() << "},";
    dbg << "{" << o.x() << "," << o.y() << "}}";

    dbg.setFlags(flags);
    return dbg;
}

} // namespace floormat
