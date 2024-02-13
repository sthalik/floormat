#pragma once
#include "atlas-loader-fwd.hpp"
#include <memory>

namespace floormat { struct wall_cell; class wall_atlas; }

namespace floormat::loader_detail {

template<> struct atlas_loader_traits<wall_atlas>
{
    using Atlas = wall_atlas;
    using Cell = wall_cell;       // convertible to atlas and holding the atlas
    using Self = atlas_loader_traits<wall_atlas>;
    using Storage = atlas_storage<wall_atlas, Self>;

    static StringView loader_name();
    static const std::shared_ptr<Atlas>& atlas_of(const Cell& x);
    static std::shared_ptr<Atlas>& atlas_of(Cell& x);
    static StringView name_of(const Cell& x);
    static String& name_of(Cell& x);
    static void atlas_list(Storage& st);
    static Cell make_invalid_atlas(Storage& st);
    static std::shared_ptr<Atlas> make_atlas(StringView name, const Cell& c);
    static Optional<Cell> make_cell(StringView name);
};

} // namespace floormat::loader_detail
