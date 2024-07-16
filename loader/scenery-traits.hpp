#pragma once
#include "atlas-loader-fwd.hpp"
#include "compat/borrowed-ptr.hpp"

namespace floormat { struct scenery_cell; struct scenery_proto; }

namespace floormat::loader_detail {

template<> struct atlas_loader_traits<scenery_proto>
{
    using Atlas = scenery_proto;
    using Cell = scenery_cell;
    using Self = atlas_loader_traits<Atlas>;
    using Storage = atlas_storage<Atlas, Self>;

    static StringView loader_name();
    static const Optional<Atlas>& atlas_of(const Cell& x);
    static Optional<Atlas>& atlas_of(Cell& x);
    static StringView name_of(const Cell& x);
    static String& name_of(Cell& x);
    static void atlas_list(Storage& st);
    static Cell make_invalid_atlas(Storage& st);
    static Optional<scenery_proto> make_atlas(StringView name, const Cell& c);
    static Optional<Cell> make_cell(StringView name);
};

} // namespace floormat::loader_detail
