#pragma once
#include "tile-constants.hpp"
#include "global-coords.hpp"
#include "object-id.hpp"
#include "search-result.hpp"
#include "search-pred.hpp"
#include <array>

namespace floormat::Search {
template<typename T> struct bbox;
struct cache;
} // namespace floormat::Search

namespace floormat {

class world;
struct object;
class chunk;
struct path_search_result;

class path_search final
{
    friend struct path_search_result;
    template<typename T> using bbox = Search::bbox<T>;
    using pred = Search::pred;

public:
    static bool is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id, const pred& p = Search::never_continue());
    static bool is_passable_(chunk* c0, const std::array<chunk*, 8>& neighbors,
                             Vector2 min, Vector2 max, object_id own_id, const pred& p = Search::never_continue());
    static bool is_passable(world& w, global_coords coord, Vector2b offset, Vector2ui size, object_id own_id, const pred& p = Search::never_continue());
    static bool is_passable(world& w, struct Search::cache& cache, global_coords coord, Vector2b offset, Vector2ui size, object_id own_id, const pred& p = Search::never_continue());
    static bool is_passable(world& w, chunk_coords_ ch0, const bbox<float>& bb, object_id own_id, const pred& p = Search::never_continue());
    static bool is_passable(world& w, struct Search::cache& cache, chunk_coords_ ch0, const bbox<float>& bb, object_id own_id, const pred& p = Search::never_continue());
};

} // namespace floormat
