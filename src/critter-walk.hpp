#pragma once
#include "critter.hpp"
#include "search-result.hpp"
#include "search-pred.hpp"

namespace floormat::Grid::Pass { class Pool; }
namespace floormat { struct grid_order; }

namespace floormat {

// Walks a critter to a point, or along a path search's result, smoothed or not.
class critter_walk
{
public:
    using walk_result = critter::walk_result;

    critter_walk();
    explicit critter_walk(point dest);
    explicit critter_walk(path_search_result path);

    // Spends dt walking. walk_result::none until the walk ends.
    walk_result update(const bptr<critter>& c, usize& i, const Ns& dt);
    point dest() const;
    // True for a walk built from a point, straight only when the point lies on an axis or diagonal from the start.
    bool is_line() const;
    // True when c can move its box along the rest of a smoothed path as move_toward() would, with no collider but
    // c in the way. Other walks return true untested, as their move_toward() slides past blocked diagonal steps.
    bool is_remaining_path_unblocked(critter& c) const;
    // As above, with only the colliders p blocks in the way. With pool, whose grid holds no critters, p must let
    // every critter through.
    bool is_remaining_path_unblocked(critter& c, const Search::pred& p, Grid::Pass::Pool* pool) const;
    // As above, writing neither c's world nor pool, as the read-only find_swept_collider() does. Without pool, or
    // on a chunk without its built grid, the R-tree test counts critters p blocks.
    bool is_remaining_path_unblocked(const critter& c, const Search::pred& p, const Grid::Pass::Pool* pool,
                                     Array<grid_order>& orders) const;

private:
    enum class walk_mode : u8 { none, line, path, smoothed, };

    // The body of the overloads taking p. A writable call passes c again as writable_critter, pool again as
    // writable_pool, and a null orders. A read-only call passes null writable pointers.
    bool is_remaining_path_unblocked(const critter& c, critter* writable_critter, const Search::pred& p,
                                     const Grid::Pass::Pool* pool, Grid::Pass::Pool* writable_pool,
                                     Array<grid_order>* orders) const;

    walk_result walk_line(const bptr<critter>& c, usize& i, const Ns& dt);
    walk_result walk_path(const bptr<critter>& c, usize& i, const Ns& dt);
    walk_result walk_smoothed(const bptr<critter>& c, usize& i, const Ns& dt);

    point _dest;
    path_search_result path;
    u32 path_index = -1u;
    walk_mode mode = walk_mode::none;
};

} // namespace floormat
