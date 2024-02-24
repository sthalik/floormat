#pragma once
#include "tile-constants.hpp"
#include "global-coords.hpp"
#include "object-id.hpp"
#include "world.hpp"
#include "compat/function2.fwd.hpp"
#include "path-search-result.hpp"
#include <concepts>
#include <array>
#include <Magnum/Math/Vector2.h>
#include <Magnum/DimensionTraits.h>

namespace floormat {
class world;
struct object;
class chunk;
} // namespace floormat

// todo add pathfinding sub-namespace

namespace floormat::detail_astar {

struct cache;
struct chunk_cache;
static constexpr int div_factor = 4;
static constexpr auto div_size = iTILE_SIZE2 / div_factor;
static constexpr auto min_size = Vector2ui(div_size * 2);

} // namespace floormat::detail_astar

namespace floormat {

struct path_search_result;
enum class path_search_continue : bool { pass = false, blocked = true };

class path_search final
{
    friend struct path_search_result;

public:
    template<typename T>
    requires std::is_arithmetic_v<T>
    struct bbox
    {
        VectorTypeFor<2, T> min, max;

        template<typename U>
        requires std::is_arithmetic_v<U>
        explicit constexpr operator bbox<U>() const {
            using Vec = VectorTypeFor<2, U>;
            return bbox<U>{ Vec(min), Vec(max) };
        }

        constexpr bool operator==(const bbox<T>&) const = default;
    };

    using pred = fu2::function_view<path_search_continue(collision_data) const>;

    static const pred& never_continue() noexcept;
    static const pred& always_continue() noexcept;

    static bool is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable_(chunk* c0, const std::array<chunk*, 8>& neighbors,
                             Vector2 min, Vector2 max, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, global_coords coord, Vector2b offset, Vector2ui size, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, struct detail_astar::cache& cache, global_coords coord, Vector2b offset, Vector2ui size, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, chunk_coords_ ch0, const bbox<float>& bb, object_id own_id, const pred& p = never_continue());
    static bool is_passable(world& w, struct detail_astar::cache& cache, chunk_coords_ ch0, const bbox<float>& bb, object_id own_id, const pred& p = never_continue());
};



} // namespace floormat
