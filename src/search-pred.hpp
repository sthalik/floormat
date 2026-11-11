#pragma once
#include "compat/function2.fwd.hpp"
#include "collision.hpp"
#include "object-id.hpp"

namespace floormat {

class chunk;

enum class path_search_continue : bool { pass = false, blocked = true };

} // namespace floormat

namespace floormat::Search {

// Range2D is in `Chunk&`'s own chunk-local frame, not the original query's;
// cross-chunk consumers must lift via (self.coord() - query_chunk.coord()) * chunk_size.
template<typename Chunk = chunk> using Pred = fu2::function_view<path_search_continue(Chunk&,collision_data,Range2D)const>;
using pred = Pred<chunk>;

template<typename Chunk = chunk> const Pred<Chunk>& never_continue() noexcept;
template<typename Chunk = chunk> const Pred<Chunk>& always_continue() noexcept;
template<typename Chunk = chunk> const Pred<Chunk>& without_critters() noexcept;
template<typename Chunk = chunk> const Pred<Chunk>& without_dynamic_objects() noexcept;

// Every collider but self blocks. A pred made from the returned lambda must not outlive it.
inline auto blocks_all_but(object_id self)
{
    return [self](chunk&, collision_data x, const Range2D&) {
        return x.id == self ? path_search_continue::pass : path_search_continue::blocked;
    };
}

} // namespace floormat::Search
