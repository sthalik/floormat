#pragma once
#include "atlas-loader-fwd.hpp"
#include "compat/borrowed-ptr.hpp"
#include <cr/Optional.h>

namespace floormat { struct anim_cell; class anim_atlas; }

namespace floormat::loader_detail {

template<> struct atlas_loader_traits<anim_atlas>
{
    using Atlas = anim_atlas;
    using Cell = anim_cell;       // convertible to atlas and holding the atlas
    using Self = atlas_loader_traits<Atlas>;
    using Storage = atlas_storage<Atlas, Self>;

    static StringView loader_name();
    static const bptr<Atlas>& atlas_of(const Cell& x);
    static bptr<Atlas>& atlas_of(Cell& x);
    static StringView name_of(const Cell& x);
    static String& name_of(Cell& x);
    static void atlas_list(Storage& st);
    static Cell make_invalid_atlas(Storage& st);
    static bptr<Atlas> make_atlas(StringView name, const Cell& c);
    static Optional<Cell> make_cell(StringView name);
};

} // namespace floormat::loader_detail
