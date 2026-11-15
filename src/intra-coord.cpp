#include "intra-coord.hpp"
#include "compat/debug.hpp"

namespace floormat {

template<intra_coord_base::Type T>
Debug& operator<<(Debug& dbg, const basic_intra_coord<T>& val)
{
    return dbg << cat(T == intra_coord_base::Checking ? "intra_coord{" : "intra_coord<Wrapping>{",
                      val.x(), ",", val.y(), "}");
}

template Debug& operator<<(Debug& dbg, const basic_intra_coord<intra_coord_base::Checking>& val);
template Debug& operator<<(Debug& dbg, const basic_intra_coord<intra_coord_base::Wrapping>& val);

} // namespace floormat
