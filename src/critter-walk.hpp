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
    walk_result update(const bptr<critter>& c, size_t& i, const Ns& dt);
    point dest() const;

private:
    enum class walk_mode : uint8_t { none, line, path, smoothed, };

    walk_result walk_line(const bptr<critter>& c, size_t& i, const Ns& dt);
    walk_result walk_path(const bptr<critter>& c, size_t& i, const Ns& dt);
    walk_result walk_smoothed(const bptr<critter>& c, size_t& i, const Ns& dt);

    point _dest;
    path_search_result path;
    uint32_t path_index = -1u;
    walk_mode mode = walk_mode::none;
};

} // namespace floormat
