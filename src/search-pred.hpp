#pragma once
#include "compat/function-view-fwd.hpp"
#include "collision.hpp"
#include "object-id.hpp"

namespace floormat {

class chunk;
struct object;

enum class path_search_continue : bool { pass = false, blocked = true };

} // namespace floormat

namespace floormat::Search {

using pred = function_view<path_search_continue(const chunk&,collision_data)const>;
// Range2D is in `const chunk&`'s own chunk-local frame, not the original query's;
// cross-chunk consumers must lift via (self.coord() - query_chunk.coord()) * chunk_size.
using collider_visitor = function_view<path_search_continue(const chunk&,collision_data,Range2D)const>;

constexpr path_search_continue never_continue(const chunk&, collision_data) { return path_search_continue::blocked; }
path_search_continue without_critters(const chunk& self, collision_data data);
path_search_continue without_dynamic_objects(const chunk& self, collision_data data);

bool is_dynamic_object(const object& o);
bool is_dynamic_object(const chunk& c, collision_data x);

// Every collider but self blocks. A pred made from the returned lambda must not outlive it.
inline auto blocks_all_but(object_id self)
{
    return [self](const chunk&, collision_data x) {
        return x.id == self ? path_search_continue::pass : path_search_continue::blocked;
    };
}

} // namespace floormat::Search
