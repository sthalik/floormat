#pragma once
#include "atlas-loader-fwd.hpp"
#include <memory>

namespace floormat { struct ground_cell; class ground_atlas; }

namespace floormat::loader_detail {

template<> struct atlas_loader_traits<ground_atlas>
{
    using Atlas = ground_atlas;
    using Cell = ground_cell;       // convertible to atlas and holding the atlas
    using Self = atlas_loader_traits<ground_atlas>;
    using Storage = atlas_storage<ground_atlas, Self>;

    static StringView loader_name();
    static const std::shared_ptr<Atlas>& atlas_of(const Cell& x);
    static std::shared_ptr<Atlas>& atlas_of(Cell& x);
    static StringView name_of(const Cell& x);
    static StringView name_of(const Atlas& x);
    static String& name_of(Cell& x);
    static void load_atlas_list(Storage& s);
    static Pointer<Cell> make_invalid_atlas(Storage& st);
    static std::shared_ptr<Atlas> make_atlas(StringView name, const Cell& c);
    static Optional<Cell> make_cell(StringView name);
};

} // namespace floormat::loader_detail
