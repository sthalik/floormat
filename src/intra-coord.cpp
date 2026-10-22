#include "intra-coord.hpp"

namespace floormat {

template<intra_coord_base::Type T>
Debug& operator<<(Debug& dbg, const basic_intra_coord<T>& val)
{
    dbg << "";
    const auto flags = dbg.flags();
    dbg.setFlags(flags | Debug::Flag::NoSpace);
    dbg << (T == intra_coord_base::Checking ? "intra_coord{" : "intra_coord<Wrapping>{") << val.x() << "," << val.y() << "}";
    dbg.setFlags(flags);
    return dbg;
}

template Debug& operator<<(Debug& dbg, const basic_intra_coord<intra_coord_base::Checking>& val);
template Debug& operator<<(Debug& dbg, const basic_intra_coord<intra_coord_base::Wrapping>& val);

} // namespace floormat
