#pragma once
#include "critter.hpp"
#include "search-result.hpp"

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

private:
    enum class walk_mode : u8 { none, line, path, smoothed, };

    walk_result walk_line(const bptr<critter>& c, usize& i, const Ns& dt);
    walk_result walk_path(const bptr<critter>& c, usize& i, const Ns& dt);
    walk_result walk_smoothed(const bptr<critter>& c, usize& i, const Ns& dt);

    point _dest;
    path_search_result path;
    u32 path_index = -1u;
    walk_mode mode = walk_mode::none;
};

} // namespace floormat
